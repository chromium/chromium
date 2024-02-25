// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_BASE_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_BASE_H_

#include "components/cast_receiver/browser/streaming_controller.h"

#include <memory>

#include "base/sequence_checker.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace cast_api_bindings {
class MessagePort;
}  // namespace cast_api_bindings

namespace content {
class NavigationHandle;
}  // namespace content

namespace cast_receiver {

// This class provides an implementation of StreamingController using the types
// provided in the cast_streaming component.
class StreamingControllerBase : public StreamingController,
                                public content::WebContentsObserver {
 public:
  static std::unique_ptr<StreamingController> Create(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      content::WebContents* web_contents);

  ~StreamingControllerBase() override;

 protected:
  StreamingControllerBase(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      content::WebContents* web_contents);

  // Begins playback of |receiver_session|.
  virtual void StartPlayback(
      cast_streaming::ReceiverSession* receiver_session,
      mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
          demuxer_connector,
      mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
          renderer_connection) = 0;

  // Makes any modifications or validations to |config| needed prior to the
  // initialization of the streaming receiver.
  virtual void ProcessConfig(cast_streaming::ReceiverConfig& config);

 private:
  // content::WebContentsObserver overrides:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) final;

  // Partial StreamingController overrides:
  void InitializeReceiverSession(
      cast_streaming::ReceiverConfig config,
      cast_streaming::ReceiverSession::Client* client) final;
  void StartPlaybackAsync(PlaybackStartedCB cb) final;

  // Starts playback if all of the following have occurred:
  // - CreateReceiverSession() has been called.
  // - StartPlaybackAsync() has been called.
  // - The page associated with |cast_web_contents| as provided in the ctor is
  //   ready to commit navigation.
  void TryStartPlayback();

  SEQUENCE_CHECKER(sequence_checker_);

  // Populated in StartPlaybackAsync().
  PlaybackStartedCB playback_started_cb_;

  // Populated in InitializeReceiverSession()
  std::optional<cast_streaming::ReceiverConfig> config_ = std::nullopt;
  cast_streaming::ReceiverSession::Client* client_ = nullptr;

  // Mojo connections. Initially populated in MainFrameReadyToCommitNavigation()
  // with connections to the Renderer process, and transferred to
  // StartPlayback() when it is first called.
  mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
      demuxer_connector_;
  mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
      renderer_connection_;

  // Populated in the ctor, and used to create |receiver_session_| in
  // TryStartPlayback().
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;

  // Created in CreateReceiverSession().
  std::unique_ptr<cast_streaming::ReceiverSession> receiver_session_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_BASE_H_
