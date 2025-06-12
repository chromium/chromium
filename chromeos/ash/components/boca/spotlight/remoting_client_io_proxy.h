// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REMOTING_CLIENT_IO_PROXY_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REMOTING_CLIENT_IO_PROXY_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "remoting/client/common/client_status_observer.h"
#include "remoting/protocol/frame_consumer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace network {
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {
class RemotingClient;
}  // namespace remoting

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace ash::boca {

// Class used to run the `remoting::RemotingClient` on the IO sequence.
class RemotingClientIOProxy : public remoting::ClientStatusObserver {
 public:
  RemotingClientIOProxy(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
      SpotlightCrdStateUpdatedCallback status_updated_callback);
  RemotingClientIOProxy(const RemotingClientIOProxy&) = delete;
  RemotingClientIOProxy& operator=(const RemotingClientIOProxy&) = delete;
  ~RemotingClientIOProxy() override;

  // `remoting::ClientStatusObserver`
  void OnConnectionFailed() override;
  void OnConnected() override;
  void OnDisconnected() override;
  void OnClientDestroyed() override;

  // Starts a `remoting::RemotingClient`
  void StartCrdClient(std::string crd_connection_code,
                      std::string oauth_access_token,
                      std::string authorized_helper_email,
                      base::OnceClosure crd_session_ended_callback);

  // Stops the `remoting::RemotingClient` if there is an active session and
  // releases the resources for the next session.
  void StopCrdClient();

 private:
  // Accepts the CRD ended event on the current sequence and forwards it to the
  // `crd_session_ended_callback_`.
  void HandleCrdSessionEnded();

  // Update the `status_updated_callback_` with the CrdConnectionState.
  void UpdateState(CrdConnectionState state);

  // Receives the frame on the current sequence and forwards it to the
  // `frame_recieved_callback_`.
  void OnFrameReceived(SkBitmap bitmap,
                       std::unique_ptr<webrtc::DesktopFrame> frame);

  // Releases the `remoting::RemoteClient` and `SpotlightFrameConsumer` used
  // for a previous session.
  void ResetRemotingClient(
      std::unique_ptr<remoting::RemotingClient> remoting_client,
      std::unique_ptr<SpotlightFrameConsumer> frame_consumer);

  SEQUENCE_CHECKER(sequence_checker_);

  // Callback for handling an update that the crd session has ended.
  base::OnceClosure crd_session_ended_callback_;
  // Callback for receiving a completed frame from `SpotlightFrameConsumer`.
  SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback_;
  // Callback for `CrdConnectionState` updates.
  SpotlightCrdStateUpdatedCallback status_updated_callback_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<SpotlightFrameConsumer> frame_consumer_;
  std::unique_ptr<remoting::RemotingClient> remoting_client_;

  base::WeakPtrFactory<RemotingClientIOProxy> weak_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REMOTING_CLIENT_IO_PROXY_H_
