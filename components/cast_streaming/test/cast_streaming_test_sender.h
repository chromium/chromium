// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_TEST_CAST_STREAMING_TEST_SENDER_H_
#define COMPONENTS_CAST_STREAMING_TEST_CAST_STREAMING_TEST_SENDER_H_

#include <optional>

#include "components/cast/message_port/message_port.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "net/base/ip_address.h"
#include "third_party/openscreen/src/cast/streaming/public/sender_session.h"

namespace cast_streaming {

class CastMessagePortSenderImpl;

// Cast Streaming Sender implementation for testing. This class provides basic
// functionality for starting a Cast Streaming Sender and sending audio and
// video frames to a Cast Streaming Receiver. Example usage:
//
//   std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
//   std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
//   cast_api_bindings::CreatePlatformMessagePortPair(&sender_message_port,
//                                                    &receiver_message_port);
//
//   CastStreamingTestSender sender;
//   sender.Start(
//       std::move(sender_message_port), net::IPAddress::IPv6Localhost(),
//       audio_config, video_config);
//   if(!sender.RunUntilActive()) {
//     return;
//   }
//   sender.SendAudioBuffer(audio_buffer);
//   sender.SendVideoBuffer(video_buffer);
//   sender.Stop();
//   sender.RunUntilStopped();
//
//   // Send |receiver_message_port| to a Receiver and start it.
class CastStreamingTestSender final
    : public openscreen::cast::SenderSession::Client {
 public:
  CastStreamingTestSender();
  ~CastStreamingTestSender() override;

  CastStreamingTestSender(const CastStreamingTestSender&) = delete;
  CastStreamingTestSender& operator=(const CastStreamingTestSender&) = delete;

  // Uses |message_port| as the Sender-end of a Cast Streaming MessagePort to
  // instantiate a Cast Streaming Session with a Cast Streaming Receiver at
  // |receiver_address|. At least one of |audio_config| or |video_config| must
  // be set.
  void Start(std::unique_ptr<cast_api_bindings::MessagePort> message_port,
             net::IPAddress receiver_address,
             std::optional<media::AudioDecoderConfig> audio_config,
             std::optional<media::VideoDecoderConfig> video_config);

  // Ends the Cast Streaming Session.
  void Stop();

  // Sends |audio_buffer| or |video_buffer| to the Receiver. These can only be
  // called when |has_startup_completed_| is true.
  void SendAudioBuffer(scoped_refptr<media::DecoderBuffer> audio_buffer);
  void SendVideoBuffer(scoped_refptr<media::DecoderBuffer> video_buffer);

  // After a call to Start(), will run until the Cast Streaming
  // Sender Session is active or has failed. After this call,
  // |has_startup_completed_| will be true. If the session started successfully,
  // at least one of audio_decoder_config() or video_decoder_config() will be
  // set, and method will return true. Otherwise returns false.
  [[nodiscard]] bool RunUntilActive();

  // Runs until |has_startup_completed_| is false.
  void RunUntilStopped();

  const std::optional<media::AudioDecoderConfig>& audio_decoder_config() const {
    return audio_decoder_config_;
  }
  const std::optional<media::VideoDecoderConfig>& video_decoder_config() const {
    return video_decoder_config_;
  }

 private:
  class SenderObserver;

  void OnCastChannelClosed();

  // After a system sender message is received, negotiates Cast Streaming Sender
  // Session with given audio and video configs.
  void OnSystemSenderMessageReceived();

  // openscreen::cast::SenderSession::Client implementation.
  void OnNegotiated(const openscreen::cast::SenderSession* session,
                    openscreen::cast::SenderSession::ConfiguredSenders senders,
                    openscreen::cast::capture_recommendations::Recommendations
                        capture_recommendations) final;
  void OnError(const openscreen::cast::SenderSession* session,
               const openscreen::Error& error) final;

  openscreen_platform::TaskRunner task_runner_;
  openscreen::cast::Environment environment_;

  std::unique_ptr<CastMessagePortSenderImpl> message_port_;
  std::unique_ptr<openscreen::cast::SenderSession> sender_session_;

  bool has_startup_completed_ = false;
  std::optional<media::AudioDecoderConfig> audio_decoder_config_;
  std::optional<media::VideoDecoderConfig> video_decoder_config_;

  // Used to implement RunUntilStarted() and RunUntilStopped().
  base::OnceClosure sender_started_closure_;
  base::OnceClosure sender_stopped_closure_;

  std::vector<openscreen::cast::AudioCaptureConfig> audio_configs_;
  std::vector<openscreen::cast::VideoCaptureConfig> video_configs_;

  std::unique_ptr<SenderObserver> audio_sender_observer_;
  std::unique_ptr<SenderObserver> video_sender_observer_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_TEST_CAST_STREAMING_TEST_SENDER_H_
