// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/streaming_initialization_info.h"

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

}  // namespace cast_streaming
