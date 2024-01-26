// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/common/streaming_initialization_info.h"

#include "base/functional/callback_helpers.h"

namespace cast_streaming {

StreamingInitializationInfo::StreamingInitializationInfo() = default;

StreamingInitializationInfo::StreamingInitializationInfo(
    const openscreen::cast::ReceiverSession* receiver_session,
    std::optional<AudioStreamInfo> audio_info,
    std::optional<VideoStreamInfo> video_info,
    bool is_remoting_stream)
    : session(receiver_session),
      audio_stream_info(std::move(audio_info)),
      video_stream_info(std::move(video_info)),
      is_remoting(is_remoting_stream) {}

StreamingInitializationInfo::StreamingInitializationInfo(
    const StreamingInitializationInfo& other) = default;

StreamingInitializationInfo::~StreamingInitializationInfo() = default;

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo() = default;

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo(
    media::AudioDecoderConfig audio_config,
    openscreen::cast::Receiver* cast_receiver)
    : AudioStreamInfo(std::move(audio_config), cast_receiver, nullptr) {}

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo(
    media::AudioDecoderConfig audio_config,
    openscreen::cast::Receiver* cast_receiver,
    base::WeakPtr<DemuxerStreamClient> ds_client)
    : config(std::move(audio_config)),
      receiver(cast_receiver),
      demuxer_stream_client(std::move(ds_client)) {}

StreamingInitializationInfo::AudioStreamInfo::AudioStreamInfo(
    const StreamingInitializationInfo::AudioStreamInfo& other) = default;

StreamingInitializationInfo::AudioStreamInfo::~AudioStreamInfo() = default;

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo() = default;

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo(
    media::VideoDecoderConfig video_config,
    openscreen::cast::Receiver* cast_receiver)
    : VideoStreamInfo(std::move(video_config), cast_receiver, nullptr) {}

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo(
    media::VideoDecoderConfig video_config,
    openscreen::cast::Receiver* cast_receiver,
    base::WeakPtr<DemuxerStreamClient> ds_client)
    : config(std::move(video_config)),
      receiver(cast_receiver),
      demuxer_stream_client(std::move(ds_client)) {}

StreamingInitializationInfo::VideoStreamInfo::VideoStreamInfo(
    const StreamingInitializationInfo::VideoStreamInfo& other) = default;

StreamingInitializationInfo::VideoStreamInfo::~VideoStreamInfo() = default;

}  // namespace cast_streaming
