// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REMOTING_CLIENT_IO_PROXY_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REMOTING_CLIENT_IO_PROXY_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_audio_stream_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "remoting/client/common/client_status_observer.h"
#include "remoting/protocol/frame_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace remoting {
class AudioPacket;
struct OAuthTokenInfo;
class RemotingClient;
}  // namespace remoting

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace ash::boca {

class SpotlightAudioStreamConsumer;

class RemotingClientIOProxy {
 public:
  class Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() = default;

    virtual void OnCrdSessionEnded() = 0;
    virtual void OnStateUpdated(CrdConnectionState state) = 0;
    virtual void OnFrameReceived(
        SkBitmap bitmap,
        std::unique_ptr<webrtc::DesktopFrame> frame) = 0;
    virtual void OnAudioPacketReceived(
        std::unique_ptr<remoting::AudioPacket> packet) = 0;

   protected:
    Observer() = default;
  };

  RemotingClientIOProxy(const RemotingClientIOProxy&) = delete;
  RemotingClientIOProxy& operator=(const RemotingClientIOProxy&) = delete;

  virtual ~RemotingClientIOProxy() = default;

  // Starts a `remoting::RemotingClient`
  virtual void StartCrdClient(std::string crd_connection_code,
                              std::string oauth_access_token,
                              std::string authorized_helper_email,
                              base::WeakPtr<Observer> observer) = 0;

  // Stops the `remoting::RemotingClient` if there is an active session and
  // releases the resources for the next session.
  virtual void StopCrdClient(base::OnceClosure on_stopped_callback) = 0;

 protected:
  RemotingClientIOProxy() = default;
};

// Class used to run the `remoting::RemotingClient` on the IO sequence.
class RemotingClientIOProxyImpl : public RemotingClientIOProxy,
                                  public remoting::ClientStatusObserver {
 public:
  class RemotingClientWrapper {
   public:
    RemotingClientWrapper(const RemotingClientWrapper&) = delete;
    RemotingClientWrapper& operator=(const RemotingClientWrapper&) = delete;

    virtual ~RemotingClientWrapper() = default;

    virtual void StartSession(std::string_view support_access_code,
                              remoting::OAuthTokenInfo oauth_token_info) = 0;

    virtual void StopSession() = 0;

    virtual void AddObserver(remoting::ClientStatusObserver* observer) = 0;
    virtual void RemoveObserver(remoting::ClientStatusObserver* observer) = 0;

   protected:
    RemotingClientWrapper() = default;
  };
  using CreateRemotingClientWrapperCb =
      base::RepeatingCallback<std::unique_ptr<RemotingClientWrapper>(
          base::OnceClosure,
          std::unique_ptr<SpotlightFrameConsumer>,
          std::unique_ptr<SpotlightAudioStreamConsumer>,
          scoped_refptr<network::SharedURLLoaderFactory>)>;

  RemotingClientIOProxyImpl(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> observer_task_runner,
      CreateRemotingClientWrapperCb create_remoting_client_wrapper_cb =
          base::BindRepeating(
              &RemotingClientIOProxyImpl::CreateRemotingClientWrapper));
  RemotingClientIOProxyImpl(const RemotingClientIOProxyImpl&) = delete;
  RemotingClientIOProxyImpl& operator=(const RemotingClientIOProxyImpl&) =
      delete;
  ~RemotingClientIOProxyImpl() override;

  // `remoting::ClientStatusObserver`
  void OnConnectionFailed() override;
  void OnConnected() override;
  void OnDisconnected() override;
  void OnClientDestroyed() override;

  // RemotingClientIOProxy:
  void StartCrdClient(std::string crd_connection_code,
                      std::string oauth_access_token,
                      std::string authorized_helper_email,
                      base::WeakPtr<Observer> observer) override;
  void StopCrdClient(base::OnceClosure on_stopped_callback) override;

 private:
  static std::unique_ptr<RemotingClientWrapper> CreateRemotingClientWrapper(
      base::OnceClosure quit_closure,
      std::unique_ptr<SpotlightFrameConsumer> frame_consumer,
      std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  void UpdateState(CrdConnectionState state);

  // Releases the `RemoteClientWrapper` and `SpotlightFrameConsumer` used
  // for a previous session.
  void ResetRemotingClient(
      std::unique_ptr<RemotingClientWrapper> remoting_client_wrapper,
      base::OnceClosure on_stopped_callback);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  const scoped_refptr<base::SequencedTaskRunner> observer_task_runner_;
  base::WeakPtr<Observer> observer_;
  std::unique_ptr<RemotingClientWrapper> remoting_client_wrapper_;
  CreateRemotingClientWrapperCb create_remoting_client_wrapper_cb_;

  base::WeakPtrFactory<RemotingClientIOProxyImpl> weak_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REMOTING_CLIENT_IO_PROXY_H_
