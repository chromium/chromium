// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_audio_stream_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "remoting/client/common/remoting_client.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/frame_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {
namespace {

class RemotingClientWrapperImpl
    : public RemotingClientIOProxyImpl::RemotingClientWrapper {
 public:
  RemotingClientWrapperImpl(
      base::OnceClosure quit_closure,
      std::unique_ptr<SpotlightFrameConsumer> frame_consumer,
      std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : frame_consumer_(std::move(frame_consumer)),
        audio_stream_consumer_(std::move(audio_stream_consumer)),
        remoting_client_(std::make_unique<remoting::RemotingClient>(
            std::move(quit_closure),
            frame_consumer_.get(),
            audio_stream_consumer_ ? audio_stream_consumer_->GetWeakPtr()
                                   : nullptr,
            url_loader_factory)) {}

  RemotingClientWrapperImpl(const RemotingClientWrapperImpl&) = delete;
  RemotingClientWrapperImpl& operator=(const RemotingClientWrapperImpl&) =
      delete;

  ~RemotingClientWrapperImpl() override = default;

  void StartSession(std::string_view support_access_code,
                    remoting::OAuthTokenInfo oauth_token_info) override {
    remoting_client_->StartSession(support_access_code,
                                   std::move(oauth_token_info));
  }

  void StopSession() override { remoting_client_->StopSession(); }

  void AddObserver(remoting::ClientStatusObserver* observer) override {
    remoting_client_->AddObserver(observer);
  }

  void RemoveObserver(remoting::ClientStatusObserver* observer) override {
    remoting_client_->RemoveObserver(observer);
  }

 private:
  const std::unique_ptr<SpotlightFrameConsumer> frame_consumer_;
  const std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer_;
  const std::unique_ptr<remoting::RemotingClient> remoting_client_;
};

}  // namespace

RemotingClientIOProxyImpl::RemotingClientIOProxyImpl(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
    SpotlightAudioStreamConsumer::AudioPacketReceivedCallback
        audio_packet_received_callback,
    SpotlightCrdStateUpdatedCallback status_updated_callback,
    CreateRemotingClientWrapperCb create_remoting_client_wrapper_cb)
    : pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      frame_received_callback_(std::move(frame_received_callback)),
      audio_packet_received_callback_(
          std::move(audio_packet_received_callback)),
      status_updated_callback_(std::move(status_updated_callback)),
      create_remoting_client_wrapper_cb_(
          std::move(create_remoting_client_wrapper_cb)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemotingClientIOProxyImpl::~RemotingClientIOProxyImpl() = default;

void RemotingClientIOProxyImpl::OnConnectionFailed() {
  UpdateState(CrdConnectionState::kFailed);
}
void RemotingClientIOProxyImpl::OnConnected() {
  UpdateState(CrdConnectionState::kConnected);
}
void RemotingClientIOProxyImpl::OnDisconnected() {
  UpdateState(CrdConnectionState::kDisconnected);
}
void RemotingClientIOProxyImpl::OnClientDestroyed() {
  UpdateState(CrdConnectionState::kDisconnected);
}

void RemotingClientIOProxyImpl::StartCrdClient(
    std::string crd_connection_code,
    std::string oauth_access_token,
    std::string authorized_helper_email,
    base::OnceClosure crd_session_ended_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!shared_url_loader_factory_) {
    shared_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_url_loader_factory_));
  }
  crd_session_ended_callback_ = std::move(crd_session_ended_callback);
  auto frame_consumer = std::make_unique<SpotlightFrameConsumer>(
      base::BindRepeating(&RemotingClientIOProxyImpl::OnFrameReceived,
                          weak_factory_.GetWeakPtr()));

  // Only consume audio when the Boca Audio for Kiosk flag is enabled.
  std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer = nullptr;
  if (ash::features::IsBocaAudioForKioskEnabled()) {
    audio_stream_consumer = std::make_unique<SpotlightAudioStreamConsumer>(
        base::BindRepeating(&RemotingClientIOProxyImpl::OnAudioPacketReceived,
                            weak_factory_.GetWeakPtr()));
  }

  remoting_client_wrapper_ = create_remoting_client_wrapper_cb_.Run(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&RemotingClientIOProxyImpl::HandleCrdSessionEnded,
                         weak_factory_.GetWeakPtr())),
      std::move(frame_consumer), std::move(audio_stream_consumer),
      shared_url_loader_factory_);

  VLOG(1) << "[Boca] Starting CRD client for teacher";
  remoting_client_wrapper_->StartSession(
      crd_connection_code, {oauth_access_token, authorized_helper_email});
}

void RemotingClientIOProxyImpl::StopCrdClient(
    base::OnceClosure on_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  crd_session_ended_callback_.Reset();
  // Since we are explicitly stopping the session, remove observer first since
  // we do not need to be notified about the disconnect event.
  remoting_client_wrapper_->RemoveObserver(this);
  // Invalidate weak pointers on stop to prevent a race condition. An old
  // session's HandleCrdSessionEnded could otherwise run after a new session has
  // started, incorrectly invoking the new session's
  // crd_session_ended_callback_.
  weak_factory_.InvalidateWeakPtrs();
  remoting_client_wrapper_->StopSession();

  // The `remoting::RemotingClient` waits two seconds before sending the
  // disconnect signal to the host. This delayed task runs on the Unretained
  // object of `remoting::SignalStrategy`. We wait three seconds before
  // destructing the `remoting_client_` to prevent a crash due to the delayed
  // task trying to run on an object that was already destroyed. We move these
  // resources to free up the pointers for the next session.
  // TODO: crbug.com/424254181 - Update here and `remoting::RemotingClient` to
  // not require this delay as it is a messy work around.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RemotingClientIOProxyImpl::ResetRemotingClient,
                     weak_factory_.GetWeakPtr(),
                     std::move(remoting_client_wrapper_),
                     std::move(on_stopped_callback)),
      base::Seconds(3));
}

// static
std::unique_ptr<RemotingClientIOProxyImpl::RemotingClientWrapper>
RemotingClientIOProxyImpl::CreateRemotingClientWrapper(
    base::OnceClosure quit_closure,
    std::unique_ptr<SpotlightFrameConsumer> frame_consumer,
    std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<RemotingClientWrapperImpl>(
      std::move(quit_closure), std::move(frame_consumer),
      std::move(audio_stream_consumer), url_loader_factory);
}

void RemotingClientIOProxyImpl::HandleCrdSessionEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!crd_session_ended_callback_) {
    return;
  }

  std::move(crd_session_ended_callback_).Run();
}

void RemotingClientIOProxyImpl::UpdateState(CrdConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_updated_callback_.Run(state);
}

void RemotingClientIOProxyImpl::OnFrameReceived(
    SkBitmap bitmap,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  frame_received_callback_.Run(std::move(bitmap), std::move(frame));
}

void RemotingClientIOProxyImpl::OnAudioPacketReceived(
    std::unique_ptr<remoting::AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_packet_received_callback_.Run(std::move(packet));
}

void RemotingClientIOProxyImpl::ResetRemotingClient(
    std::unique_ptr<RemotingClientWrapper> remoting_client_wrapper,
    base::OnceClosure on_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remoting_client_wrapper.reset();
  std::move(on_stopped_callback).Run();
}

}  // namespace ash::boca
