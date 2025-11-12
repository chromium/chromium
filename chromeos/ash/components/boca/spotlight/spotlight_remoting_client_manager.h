// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_REMOTING_CLIENT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_REMOTING_CLIENT_MANAGER_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_audio_stream_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_oauth_token_fetcher.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {
class AudioPacket;
class RemotingClient;
}  // namespace remoting

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace ash::boca {

class SpotlightRemotingClientManager {
 public:
  SpotlightRemotingClientManager(const SpotlightRemotingClientManager&) =
      delete;
  SpotlightRemotingClientManager& operator=(
      const SpotlightRemotingClientManager&) = delete;

  virtual ~SpotlightRemotingClientManager() = default;

  virtual void StartCrdClient(
      std::string crd_connection_code,
      base::OnceClosure crd_session_ended_callback,
      SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
      SpotlightAudioStreamConsumer::AudioPacketReceivedCallback
          audio_packet_received_callback,
      SpotlightCrdStateUpdatedCallback status_updated_callback) = 0;

  virtual void StopCrdClient(base::OnceClosure on_stopped_callback) = 0;

  virtual std::string GetDeviceRobotEmail() = 0;

 protected:
  SpotlightRemotingClientManager() = default;
};

class SpotlightRemotingClientManagerImpl
    : public SpotlightRemotingClientManager,
      public RemotingClientIOProxy::Observer {
 public:
  using CreateRemotingIOProxyCb =
      base::RepeatingCallback<std::unique_ptr<RemotingClientIOProxy>(
          std::unique_ptr<network::PendingSharedURLLoaderFactory>,
          scoped_refptr<base::SequencedTaskRunner>)>;

  SpotlightRemotingClientManagerImpl(
      std::unique_ptr<SpotlightOAuthTokenFetcher> token_fetcher,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      CreateRemotingIOProxyCb create_remoting_io_proxy_cb =
          base::BindRepeating(&CreateRemotingIOProxy));

  SpotlightRemotingClientManagerImpl(
      const SpotlightRemotingClientManagerImpl&) = delete;
  SpotlightRemotingClientManagerImpl& operator=(
      const SpotlightRemotingClientManagerImpl&) = delete;
  ~SpotlightRemotingClientManagerImpl() override;

  // SpotlightRemotingClientManager:
  // Starts a `remoting::RemotingClient` using the `task_runner_`.
  void StartCrdClient(
      std::string crd_connection_code,
      base::OnceClosure crd_session_ended_callback,
      SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
      SpotlightAudioStreamConsumer::AudioPacketReceivedCallback
          audio_packet_received_callback,
      SpotlightCrdStateUpdatedCallback status_updated_callback) override;
  // Forwards the request to stop the crd client to the
  // `remoting_client_io_proxy_`.
  void StopCrdClient(base::OnceClosure on_stopped_callback) override;
  std::string GetDeviceRobotEmail() override;

 private:
  using SequencedRemotingClientIOProxy =
      base::SequenceBound<std::unique_ptr<RemotingClientIOProxy>>;

  static std::unique_ptr<RemotingClientIOProxy> CreateRemotingIOProxy(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> observer_task_runner);

  // Receives the OAuth token on the main/UI thread and calls the
  // `remoting_client_io_proxy_` to start the crd session.
  void HandleOAuthTokenRetrieved(std::string crd_connection_code,
                                 std::optional<std::string> oauth_access_token);

  // RemotingClientIOProxy::Observer:
  // Forwards the CRD ended event to the `crd_session_ended_callback_`.
  void OnCrdSessionEnded() override;
  // Update the `status_updated_callback_` with the CrdConnectionState.
  void OnStateUpdated(CrdConnectionState state) override;
  // Forwards the completed frame to the `frame_received_callback_`;
  void OnFrameReceived(SkBitmap bitmap,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;
  // Forwards the audio packet to the `audio_packet_received_callback_`;
  void OnAudioPacketReceived(
      std::unique_ptr<remoting::AudioPacket> packet) override;

  void Reset();

  SEQUENCE_CHECKER(sequence_checker_);
  bool session_in_progress_ = false;
  // Dedicated IO Thread to run the `remoting_client_io_proxy_`. Webrtc
  // requires a thread marked as IO and the Browser IO thread is already
  // incredibly busy so we are required to start our own thread.
  base::Thread io_thread_;
  base::OnceClosure crd_session_ended_callback_;
  SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback_;
  SpotlightAudioStreamConsumer::AudioPacketReceivedCallback
      audio_packet_received_callback_;
  SpotlightCrdStateUpdatedCallback status_updated_callback_;
  std::unique_ptr<SpotlightOAuthTokenFetcher> token_fetcher_;
  // The `SpotlightRemotingClientManagerImpl` is owned by the main/UI thread
  // however the remoting_client/webrtc processes on the IO sequence.
  std::unique_ptr<SequencedRemotingClientIOProxy> remoting_client_io_proxy_;
  base::OneShotTimer frame_timeout_timer_;

  base::WeakPtrFactory<SpotlightRemotingClientManagerImpl> weak_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_REMOTING_CLIENT_MANAGER_H_
