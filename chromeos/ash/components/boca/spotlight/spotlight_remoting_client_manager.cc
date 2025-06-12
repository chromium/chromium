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
#include "base/functional/callback_forward.h"
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
#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_oauth_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {

SpotlightRemotingClientManager::SpotlightRemotingClientManager(
    std::unique_ptr<SpotlightOAuthTokenFetcher> token_fetcher,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : io_thread_("Boca Spotlight IO"),
      token_fetcher_(std::move(token_fetcher)) {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  // Thread will be processing incoming video frames.
  options.thread_type = base::ThreadType::kDisplayCritical;
  CHECK(io_thread_.StartWithOptions(std::move(options)));
  remoting_client_io_proxy_ =
      std::make_unique<base::SequenceBound<RemotingClientIOProxy>>(
          io_thread_.task_runner(), url_loader_factory->Clone(),
          base::BindPostTaskToCurrentDefault(base::BindRepeating(
              &SpotlightRemotingClientManager::HandleFrameReceived,
              weak_factory_.GetWeakPtr())),
          base::BindPostTaskToCurrentDefault(
              base::BindRepeating(&SpotlightRemotingClientManager::UpdateState,
                                  weak_factory_.GetWeakPtr())));
}

SpotlightRemotingClientManager::~SpotlightRemotingClientManager() {
  // Because `remoting_client_io_proxy_` is sequence bound, the actual
  // destruction happens asynchronously on its task runner. Until this has
  // completed it is still possible for
  // `SpotlightRemotingClientManager::HandleFrameReceived` and
  //`SpotlightRemotingClientManager::UpdateState` to be called.
  remoting_client_io_proxy_.reset();
}

void SpotlightRemotingClientManager::StartCrdClient(
    std::string crd_connection_code,
    base::OnceClosure crd_session_ended_callback,
    SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
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
  status_updated_callback_ = std::move(status_updated_callback);

  token_fetcher_->Start((base::BindOnce(
      &SpotlightRemotingClientManager::HandleOAuthTokenRetrieved,
      weak_factory_.GetWeakPtr(), std::move(crd_connection_code))));
}

void SpotlightRemotingClientManager::StopCrdClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session_in_progress_) {
    return;
  }

  remoting_client_io_proxy_->AsyncCall(&RemotingClientIOProxy::StopCrdClient);

  crd_session_ended_callback_.Reset();
  frame_received_callback_.Reset();
  status_updated_callback_.Reset();
  session_in_progress_ = false;
}

std::string SpotlightRemotingClientManager::GetDeviceRobotEmail() {
  return token_fetcher_->GetDeviceRobotEmail();
}

void SpotlightRemotingClientManager::HandleOAuthTokenRetrieved(
    std::string crd_connection_code,
    std::optional<std::string> oauth_access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!oauth_access_token.has_value() || oauth_access_token->empty()) {
    LOG(ERROR) << "[Boca] Failed to retrieve OAuth token for Spotlight";
    UpdateState(CrdConnectionState::kFailed);
    return;
  }

  remoting_client_io_proxy_->AsyncCall(&RemotingClientIOProxy::StartCrdClient)
      .WithArgs(std::move(crd_connection_code),
                std::move(oauth_access_token.value()), GetDeviceRobotEmail(),
                base::BindPostTaskToCurrentDefault(base::BindOnce(
                    &SpotlightRemotingClientManager::HandleCrdSessionEnded,
                    weak_factory_.GetWeakPtr())));
}

void SpotlightRemotingClientManager::HandleCrdSessionEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!crd_session_ended_callback_) {
    return;
  }
  std::move(crd_session_ended_callback_).Run();
  frame_received_callback_.Reset();
  status_updated_callback_.Reset();
  session_in_progress_ = false;
}

void SpotlightRemotingClientManager::UpdateState(CrdConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status_updated_callback_) {
    return;
  }
  status_updated_callback_.Run(state);
}

void SpotlightRemotingClientManager::HandleFrameReceived(
    SkBitmap bitmap,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!frame_received_callback_) {
    return;
  }
  frame_received_callback_.Run(std::move(bitmap), std::move(frame));
}

}  // namespace ash::boca
