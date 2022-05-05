// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/streaming_initialization_info.h"

#include "base/callback_helpers.h"

namespace cast_streaming {

StreamingInitializationInfo::StreamingInitializationInfo() = default;

StreamingInitializationInfo::StreamingInitializationInfo(
    const openscreen::cast::ReceiverSession* receiver_session,
    absl::optional<AudioStreamInfo> audio_info,
    absl::optional<VideoStreamInfo> video_info)
    : session(receiver_session),
      audio_stream_info(std::move(audio_info)),
      video_stream_info(std::move(video_info)) {}

StreamingInitializationInfo::StreamingInitializationInfo(
    const StreamingInitializationInfo& other) = default;

StreamingInitializationInfo::~StreamingInitializationInfo() = default;

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo() = default;

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo(
    media::AudioDecoderConfig audio_config,
    openscreen::cast::Receiver* cast_receiver)
    : AudioStreamInfo(std::move(audio_config),
                      cast_receiver,
                      base::RepeatingClosure(),
                      base::OnceClosure()) {}

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo(
    media::AudioDecoderConfig audio_config,
    openscreen::cast::Receiver* cast_receiver,
    base::RepeatingClosure on_no_buffers_cb,
    base::OnceClosure on_error_cb)
    : config(std::move(audio_config)),
      receiver(cast_receiver),
      on_no_buffers_callback(std::move(on_no_buffers_cb)),
      on_error_callback(std::move(on_error_cb)) {}

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo(
    const StreamingInitializationInfo::AudioStreamInfo& other) {
  auto& old = const_cast<AudioStreamInfo&>(other);
  auto cb_pair = base::SplitOnceCallback(std::move(old.on_error_callback));
  old.on_error_callback = std::move(cb_pair.first);

  config = other.config;
  receiver = other.receiver;
  on_no_buffers_callback = other.on_no_buffers_callback;
  on_error_callback = std::move(cb_pair.second);
}

StreamingInitializationInfo::AudioStreamInfo::~AudioStreamInfo() = default;

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo() = default;

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo(
    media::VideoDecoderConfig video_config,
    openscreen::cast::Receiver* cast_receiver)
    : VideoStreamInfo(std::move(video_config),
                      cast_receiver,
                      base::RepeatingClosure(),
                      base::OnceClosure()) {}

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo(
    media::VideoDecoderConfig video_config,
    openscreen::cast::Receiver* cast_receiver,
    base::RepeatingClosure on_no_buffers_cb,
    base::OnceClosure on_error_cb)
    : config(std::move(video_config)),
      receiver(cast_receiver),
      on_no_buffers_callback(std::move(on_no_buffers_cb)),
      on_error_callback(std::move(on_error_cb)) {}

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo(
    const StreamingInitializationInfo::VideoStreamInfo& other) {
  auto& old = const_cast<VideoStreamInfo&>(other);
  auto cb_pair = base::SplitOnceCallback(std::move(old.on_error_callback));
  old.on_error_callback = std::move(cb_pair.first);

  config = other.config;
  receiver = other.receiver;
  on_no_buffers_callback = other.on_no_buffers_callback;
  on_error_callback = std::move(cb_pair.second);
}

StreamingInitializationInfo::VideoStreamInfo::~VideoStreamInfo() = default;

}  // namespace cast_streaming
