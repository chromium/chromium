// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_SESSION_CLIENT_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_SESSION_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_receiver/browser/public/streaming_config_manager.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace content {
class WebContents;
}  // namespace content

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
struct VideoTransformation;
}  // namespace media

namespace cast_receiver {

class StreamingController;

// This class wraps all //components/cast_streaming functionality, only
// expecting the caller to supply a MessagePortFactory. Internally, it
// manages the lifetimes of cast streaming objects, and informs the caller
// of important events. Methods in this class may not be called in parallel.
class StreamingReceiverSessionClient
    : public cast_receiver::StreamingConfigManager::ConfigObserver,
      public cast_streaming::ReceiverSession::Client {
 public:
  class Handler {
   public:
    virtual ~Handler();

    //.Called when the streaming session as successfully been initialized,
    // following navigation of the observed CastWebContents to a cast-supporting
    // URL.
    virtual void OnStreamingSessionStarted() = 0;

    // Called when a nonrecoverable error occurs. Following this call, the
    // associated StreamingReceiverSessionClient instance will be placed in an
    // undefined state.
    virtual void OnError() = 0;

    // Called when the resolution as reported to the media pipeline changes.
    virtual void OnResolutionChanged(
        const gfx::Rect& size,
        const ::media::VideoTransformation& transformation) = 0;
  };

  // Max time for which streaming may wait for AV Settings receipt before being
  // treated as a failure.
  static constexpr base::TimeDelta kMaxAVSettingsWaitTime = base::Seconds(5);

  // Creates a new instance of this class. |handler| must persist for the
  // lifetime of this instance.
  StreamingReceiverSessionClient(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      network::NetworkContextGetter network_context_getter,
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      content::WebContents* web_contents,
      Handler* handler,
      cast_receiver::StreamingConfigManager* config_manager,
      bool supports_audio,
      bool supports_video);

  ~StreamingReceiverSessionClient() override;

  // Schedules starting the Streaming Receiver owned by this instance. May only
  // be called once. At time of calling, this instance will be set as the
  // observer of |web_contents|, for which streaming will be started following
  // the latter of:
  // - Navigation to an associated URL by |cast_web_contents| as provided in the
  //   ctor.
  // - Receipt of supported AV Settings.
  // Following this call, the supported AV Settings are expected to remain
  // constant. If valid AV Settings have not been received within
  // |kMaxAVSettingsWaitTime| of this function call, it will be treated as an
  // unrecoverable error, and this instance will be placed in an undefined
  // state.
  void LaunchStreamingReceiverAsync();

  bool has_streaming_launched() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return streaming_state_ == LaunchState::kLaunched;
  }

  bool is_streaming_launch_pending() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return streaming_state_ & LaunchState::kLaunchCalled;
  }

  bool has_received_av_settings() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return streaming_state_ & LaunchState::kAVSettingsReceived;
  }

  bool is_healthy() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !(streaming_state_ & LaunchState::kError);
  }

 private:
  friend class StreamingReceiverSessionClientTest;

  enum LaunchState : int32_t {
    kStopped = 0x00,

    // The two conditions which must be met for streaming to run.
    kLaunchCalled = 0x01 << 0,
    kAVSettingsReceived = 0x01 << 1,

    // Signifies that the above conditions have all been met.
    kReady = kAVSettingsReceived | kLaunchCalled,

    // Signifies that streaming has started.
    kLaunched = 0xFF,

    // Error state set after a Handler::OnError() call.
    kError = 0x100
  };

  // This second ctor is required for Unit Testing.
  StreamingReceiverSessionClient(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      network::NetworkContextGetter network_context_getter,
      std::unique_ptr<StreamingController> streaming_controller,
      Handler* handler,
      cast_receiver::StreamingConfigManager* config_manager,
      bool supports_audio,
      bool supports_video);

  friend inline LaunchState operator&(LaunchState first, LaunchState second) {
    return static_cast<LaunchState>(static_cast<int32_t>(first) &
                                    static_cast<int32_t>(second));
  }

  friend inline LaunchState operator|(LaunchState first, LaunchState second) {
    return static_cast<LaunchState>(static_cast<int32_t>(first) |
                                    static_cast<int32_t>(second));
  }

  friend inline LaunchState& operator|=(LaunchState& first,
                                        LaunchState second) {
    return first = first | second;
  }

  friend inline LaunchState& operator&=(LaunchState& first,
                                        LaunchState second) {
    return first = first & second;
  }

  void TriggerError();

  // cast_streaming::ReceiverSession::Client overrides.
  void OnAudioConfigUpdated(
      const ::media::AudioDecoderConfig& audio_config) override;
  void OnVideoConfigUpdated(
      const ::media::VideoDecoderConfig& video_config) override;
  void OnStreamingSessionEnded() override;

  // cast_receiver::StreamingConfigManager::ConfigObserver overrides.
  void OnStreamingConfigSet(
      const cast_streaming::ReceiverConfig& config) override;

  void VerifyAVSettingsReceived();

  // Callback passed when calling StreamingController::StartPlayback().
  void OnPlaybackStarted();

  // Handler for callbacks associated with this class. May be empty.
  Handler* const handler_;

  // Task runner on which waiting for the result of an AV Settings query should
  // occur.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Responsible for initiating the streaming session and controlling its
  // playback state.
  std::unique_ptr<StreamingController> streaming_controller_;

  // Current state in initialization of |receiver_session_|.
  LaunchState streaming_state_ = LaunchState::kStopped;

  // Tracks if this session should be initiated as audio or video only.
  bool supports_audio_ = true;
  bool supports_video_ = true;

  base::WeakPtrFactory<StreamingReceiverSessionClient> weak_factory_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_SESSION_CLIENT_H_
