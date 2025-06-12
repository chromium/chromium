// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_REMOTING_CLIENT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_REMOTING_CLIENT_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_oauth_token_fetcher.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {
class RemotingClient;
}  // namespace remoting

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace ash::boca {

class SpotlightRemotingClientManager {
 public:
  SpotlightRemotingClientManager(
      std::unique_ptr<SpotlightOAuthTokenFetcher> token_fetcher,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  SpotlightRemotingClientManager(const SpotlightRemotingClientManager&) =
      delete;
  SpotlightRemotingClientManager& operator=(
      const SpotlightRemotingClientManager&) = delete;
  ~SpotlightRemotingClientManager();

  // Starts a `remoting::RemotingClient` using the `task_runner_`.
  void StartCrdClient(
      std::string crd_connection_code,
      base::OnceClosure crd_session_ended_callback,
      SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
      SpotlightCrdStateUpdatedCallback status_updated_callback);

  // Forwards the request to stop the crd client to the
  // `remoting_client_io_proxy_`.
  void StopCrdClient();

  std::string GetDeviceRobotEmail();

 private:
  // Receives the OAuth token on the main/UI thread and calls the
  // `remoting_client_io_proxy_` to start the crd session.
  void HandleOAuthTokenRetrieved(std::string crd_connection_code,
                                 std::optional<std::string> oauth_access_token);

  // Forwards the CRD ended event to the `crd_session_ended_callback_`.
  void HandleCrdSessionEnded();

  // Update the `status_updated_callback_` with the CrdConnectionState.
  void UpdateState(CrdConnectionState state);

  // Forwards the completed frame to the `frame_received_callback_`;
  void HandleFrameReceived(SkBitmap bitmap,
                           std::unique_ptr<webrtc::DesktopFrame> frame);

  SEQUENCE_CHECKER(sequence_checker_);
  bool session_in_progress_ = false;
  // Dedicated IO Thread to run the `remoting_client_io_proxy_`. Webrtc
  // requires a thread marked as IO and the Browser IO thread is already
  // incredibly busy so we are required to start our own thread.
  base::Thread io_thread_;
  base::OnceClosure crd_session_ended_callback_;
  SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback_;
  SpotlightCrdStateUpdatedCallback status_updated_callback_;
  std::unique_ptr<SpotlightOAuthTokenFetcher> token_fetcher_;
  // The `SpotlightRemotingClientManager` is owned by the main/UI thread
  // however the remoting_client/webrtc processes on the IO sequence.
  std::unique_ptr<base::SequenceBound<RemotingClientIOProxy>>
      remoting_client_io_proxy_;

  base::WeakPtrFactory<SpotlightRemotingClientManager> weak_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_REMOTING_CLIENT_MANAGER_H_
