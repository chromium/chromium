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
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {

RemotingClientIOProxyImpl::RemotingClientIOProxyImpl(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
    SpotlightAudioStreamConsumer::AudioPacketReceivedCallback
        audio_packet_received_callback,
    SpotlightCrdStateUpdatedCallback status_updated_callback)
    : pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      frame_received_callback_(std::move(frame_received_callback)),
      audio_packet_received_callback_(
          std::move(audio_packet_received_callback)),
      status_updated_callback_(std::move(status_updated_callback)) {
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
  frame_consumer_ = std::make_unique<SpotlightFrameConsumer>(
      base::BindRepeating(&RemotingClientIOProxyImpl::OnFrameReceived,
                          weak_factory_.GetWeakPtr()));

  // Only consume audio when the Boca Audio for Kiosk flag is enabled.
  base::WeakPtr<SpotlightAudioStreamConsumer> audio_consumer_ptr = nullptr;
  if (ash::features::IsBocaAudioForKioskEnabled()) {
    audio_stream_consumer_ = std::make_unique<SpotlightAudioStreamConsumer>(
        base::BindRepeating(&RemotingClientIOProxyImpl::OnAudioPacketReceived,
                            weak_factory_.GetWeakPtr()));
    audio_consumer_ptr = audio_stream_consumer_->GetWeakPtr();
  }

  remoting_client_ = std::make_unique<remoting::RemotingClient>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&RemotingClientIOProxyImpl::HandleCrdSessionEnded,
                         weak_factory_.GetWeakPtr())),
      frame_consumer_.get(), std::move(audio_consumer_ptr),
      shared_url_loader_factory_);

  VLOG(1) << "[Boca] Starting CRD client for teacher";
  remoting_client_->StartSession(crd_connection_code,
                                 {oauth_access_token, authorized_helper_email});
}

void RemotingClientIOProxyImpl::StopCrdClient(
    base::OnceClosure on_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  crd_session_ended_callback_.Reset();
  // Since we are explicitly stopping the session, remove observer first since
  // we do not need to be notified about the disconnect event.
  remoting_client_->RemoveObserver(this);
  remoting_client_->StopSession();

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
                     weak_factory_.GetWeakPtr(), std::move(remoting_client_),
                     std::move(frame_consumer_),
                     std::move(on_stopped_callback)),
      base::Seconds(3));
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
    std::unique_ptr<remoting::RemotingClient> remoting_client,
    std::unique_ptr<SpotlightFrameConsumer> frame_consumer,
    base::OnceClosure on_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remoting_client.reset();
  frame_consumer.reset();
  std::move(on_stopped_callback).Run();
}

}  // namespace ash::boca
