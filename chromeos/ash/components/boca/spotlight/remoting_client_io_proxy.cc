// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "remoting/client/common/remoting_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {

RemotingClientIOProxy::RemotingClientIOProxy(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
    SpotlightCrdStateUpdatedCallback status_updated_callback)
    : frame_received_callback_(std::move(frame_received_callback)),
      status_updated_callback_(std::move(status_updated_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  shared_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(pending_url_loader_factory));
}

RemotingClientIOProxy::~RemotingClientIOProxy() = default;

void RemotingClientIOProxy::OnConnectionFailed() {
  UpdateState(CrdConnectionState::kFailed);
}
void RemotingClientIOProxy::OnConnected() {
  UpdateState(CrdConnectionState::kConnected);
}
void RemotingClientIOProxy::OnDisconnected() {
  UpdateState(CrdConnectionState::kDisconnected);
}
void RemotingClientIOProxy::OnClientDestroyed() {
  UpdateState(CrdConnectionState::kDisconnected);
}

void RemotingClientIOProxy::StartCrdClient(
    std::string crd_connection_code,
    std::string oauth_access_token,
    std::string authorized_helper_email,
    base::OnceClosure crd_session_ended_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  crd_session_ended_callback_ = std::move(crd_session_ended_callback);
  frame_consumer_ =
      std::make_unique<SpotlightFrameConsumer>(base::BindRepeating(
          &RemotingClientIOProxy::OnFrameReceived, weak_factory_.GetWeakPtr()));
  remoting_client_ = std::make_unique<remoting::RemotingClient>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&RemotingClientIOProxy::HandleCrdSessionEnded,
                         weak_factory_.GetWeakPtr())),
      frame_consumer_.get(), shared_url_loader_factory_);

  VLOG(1) << "[Boca] Starting CRD client for teacher";
  remoting_client_->StartSession(crd_connection_code,
                                 {oauth_access_token, authorized_helper_email});
}

void RemotingClientIOProxy::StopCrdClient() {
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
      base::BindOnce(&RemotingClientIOProxy::ResetRemotingClient,
                     weak_factory_.GetWeakPtr(), std::move(remoting_client_),
                     std::move(frame_consumer_)),
      base::Seconds(3));
}

void RemotingClientIOProxy::HandleCrdSessionEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!crd_session_ended_callback_) {
    return;
  }

  std::move(crd_session_ended_callback_).Run();
}

void RemotingClientIOProxy::UpdateState(CrdConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_updated_callback_.Run(state);
}

void RemotingClientIOProxy::OnFrameReceived(
    SkBitmap bitmap,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  frame_received_callback_.Run(std::move(bitmap), std::move(frame));
}

void RemotingClientIOProxy::ResetRemotingClient(
    std::unique_ptr<remoting::RemotingClient> remoting_client,
    std::unique_ptr<SpotlightFrameConsumer> frame_consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remoting_client.reset();
  frame_consumer.reset();
}

}  // namespace ash::boca
