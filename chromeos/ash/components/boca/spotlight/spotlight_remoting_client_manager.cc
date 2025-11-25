// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_oauth_token_fetcher.h"
#include "remoting/proto/audio.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {

SpotlightRemotingClientManagerImpl::SpotlightRemotingClientManagerImpl(
    std::unique_ptr<SpotlightOAuthTokenFetcher> token_fetcher,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CreateRemotingIOProxyCb create_remoting_io_proxy_cb)
    : io_thread_("Boca Spotlight IO"),
      token_fetcher_(std::move(token_fetcher)) {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  // Thread will be processing incoming video frames.
  options.thread_type = base::ThreadType::kDisplayCritical;
  CHECK(io_thread_.StartWithOptions(std::move(options)));
  remoting_client_io_proxy_ = std::make_unique<SequencedRemotingClientIOProxy>(
      io_thread_.task_runner(),
      create_remoting_io_proxy_cb.Run(
          url_loader_factory->Clone(),
          /*observer_task_runner=*/base::SequencedTaskRunner::
              GetCurrentDefault()));
}

SpotlightRemotingClientManagerImpl::~SpotlightRemotingClientManagerImpl() {
  // Because `remoting_client_io_proxy_` is sequence bound, the actual
  // destruction happens asynchronously on its task runner. Until this has
  // completed it is still possible for
  // `SpotlightRemotingClientManagerImpl::HandleFrameReceived` and
  //`SpotlightRemotingClientManagerImpl::UpdateState` to be called.
  remoting_client_io_proxy_.reset();
}

void SpotlightRemotingClientManagerImpl::StartCrdClient(
    std::string crd_connection_code,
    base::OnceClosure crd_session_ended_callback,
    SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
    SpotlightAudioStreamConsumer::AudioPacketReceivedCallback
        audio_packet_received_callback,
    SpotlightCrdStateUpdatedCallback status_updated_callback) {
  CHECK(ash::features::IsBocaSpotlightRobotRequesterEnabled());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (crd_connection_code.length() != 12UL) {
    LOG(ERROR) << "[Boca] Received a connection code of invalid length while "
                  "attempting to start Spotlight CRD client: "
               << crd_connection_code;
    status_updated_callback.Run(CrdConnectionState::kFailed);
    return;
  }

  if (session_in_progress_) {
    LOG(WARNING)
        << "[Boca] Tried to initiate a Spotlight session while another "
           "was in progress";
    return;
  }
  session_in_progress_ = true;

  crd_session_ended_callback_ = std::move(crd_session_ended_callback);
  frame_received_callback_ = std::move(frame_received_callback);
  audio_packet_received_callback_ = std::move(audio_packet_received_callback);
  status_updated_callback_ = std::move(status_updated_callback);

  token_fetcher_->Start((base::BindOnce(
      &SpotlightRemotingClientManagerImpl::HandleOAuthTokenRetrieved,
      weak_factory_.GetWeakPtr(), std::move(crd_connection_code))));
}

void SpotlightRemotingClientManagerImpl::StopCrdClient(
    base::OnceClosure on_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session_in_progress_) {
    std::move(on_stopped_callback).Run();
    return;
  }
  Reset();
  remoting_client_io_proxy_->AsyncCall(&RemotingClientIOProxy::StopCrdClient)
      .WithArgs(
          base::BindPostTaskToCurrentDefault(std::move(on_stopped_callback)));
}

std::string SpotlightRemotingClientManagerImpl::GetDeviceRobotEmail() {
  return token_fetcher_->GetDeviceRobotEmail();
}

// static
std::unique_ptr<RemotingClientIOProxy>
SpotlightRemotingClientManagerImpl::CreateRemotingIOProxy(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> observer_task_runner) {
  return std::make_unique<RemotingClientIOProxyImpl>(
      std::move(pending_url_loader_factory), observer_task_runner);
}

void SpotlightRemotingClientManagerImpl::HandleOAuthTokenRetrieved(
    std::string crd_connection_code,
    std::optional<std::string> oauth_access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!oauth_access_token.has_value() || oauth_access_token->empty()) {
    LOG(ERROR) << "[Boca] Failed to retrieve OAuth token for Spotlight";
    OnStateUpdated(CrdConnectionState::kFailed);
    return;
  }

  remoting_client_io_proxy_->AsyncCall(&RemotingClientIOProxy::StartCrdClient)
      .WithArgs(std::move(crd_connection_code),
                std::move(oauth_access_token.value()), GetDeviceRobotEmail(),
                weak_factory_.GetWeakPtr());
}

void SpotlightRemotingClientManagerImpl::OnCrdSessionEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!crd_session_ended_callback_) {
    return;
  }
  base::OnceClosure session_ended_callback =
      std::move(crd_session_ended_callback_);
  Reset();
  std::move(session_ended_callback).Run();
}

void SpotlightRemotingClientManagerImpl::OnStateUpdated(
    CrdConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status_updated_callback_) {
    return;
  }
  status_updated_callback_.Run(state);
  if (state == CrdConnectionState::kTimeout) {
    // This call will be a noop if `StopCrdClient` is already called.
    StopCrdClient(base::DoNothing());
  }
}

void SpotlightRemotingClientManagerImpl::OnFrameReceived(
    SkBitmap bitmap,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!frame_received_callback_) {
    return;
  }
  constexpr base::TimeDelta kFrameTimeout = base::Seconds(5);
  frame_timeout_timer_.Stop();
  frame_timeout_timer_.Start(
      FROM_HERE, kFrameTimeout,
      base::BindOnce(&SpotlightRemotingClientManagerImpl::OnStateUpdated,
                     weak_factory_.GetWeakPtr(), CrdConnectionState::kTimeout));
  frame_received_callback_.Run(std::move(bitmap), std::move(frame));
}

void SpotlightRemotingClientManagerImpl::OnAudioPacketReceived(
    std::unique_ptr<remoting::AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!audio_packet_received_callback_) {
    return;
  }
  audio_packet_received_callback_.Run(std::move(packet));
}

void SpotlightRemotingClientManagerImpl::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  crd_session_ended_callback_.Reset();
  frame_received_callback_.Reset();
  audio_packet_received_callback_.Reset();
  status_updated_callback_.Reset();
  frame_timeout_timer_.Stop();
  session_in_progress_ = false;
}

}  // namespace ash::boca
