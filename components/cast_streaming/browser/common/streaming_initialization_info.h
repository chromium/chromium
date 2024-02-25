// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_COMMON_STREAMING_INITIALIZATION_INFO_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_COMMON_STREAMING_INITIALIZATION_INFO_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/cast_streaming/browser/common/demuxer_stream_client.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace openscreen::cast {
class Receiver;
class ReceiverSession;
}  // namespace openscreen::cast

namespace cast_streaming {

// This struct provides information pertaining to the initialization or
// re-initialization of a cast streaming session.
// NOTE: This struct IS copyable.
struct StreamingInitializationInfo {
  struct AudioStreamInfo {
    AudioStreamInfo(media::AudioDecoderConfig audio_config,
                    openscreen::cast::Receiver* cast_receiver);
    AudioStreamInfo(media::AudioDecoderConfig audio_config,
                    openscreen::cast::Receiver* cast_receiver,
                    base::WeakPtr<DemuxerStreamClient> ds_client);
    AudioStreamInfo();
    AudioStreamInfo(const AudioStreamInfo& other);
    ~AudioStreamInfo();

    // The decoder config associated with this audio stream.
    media::AudioDecoderConfig config;

    // The Receiver for the audio stream. This pointer will remain valid for the
    // duration of the streaming session.
    raw_ptr<openscreen::cast::Receiver> receiver;

    // Client with methods to be called when the DemuxerStream requires an
    // action be executed.
    base::WeakPtr<DemuxerStreamClient> demuxer_stream_client;
  };

  struct VideoStreamInfo {
    VideoStreamInfo(media::VideoDecoderConfig video_config,
                    openscreen::cast::Receiver* cast_receiver);
    VideoStreamInfo(media::VideoDecoderConfig video_config,
                    openscreen::cast::Receiver* cast_receiver,
                    base::WeakPtr<DemuxerStreamClient> ds_client);
    VideoStreamInfo();
    VideoStreamInfo(const VideoStreamInfo& other);
    ~VideoStreamInfo();

    // The decoder config associated with this video stream.
    media::VideoDecoderConfig config;

    // The Receiver for the video stream. This pointer will remain valid for the
    // duration of the streaming session.
    raw_ptr<openscreen::cast::Receiver> receiver;

    // Client with methods to be called when the DemuxerStream requires an
    // action be executed.
    base::WeakPtr<DemuxerStreamClient> demuxer_stream_client;
  };

  StreamingInitializationInfo(
      const openscreen::cast::ReceiverSession* receiver_session,
      std::optional<AudioStreamInfo> audio_info,
      std::optional<VideoStreamInfo> video_info,
      bool is_remoting);
  StreamingInitializationInfo();
  StreamingInitializationInfo(const StreamingInitializationInfo& other);
  ~StreamingInitializationInfo();

  // The receiver session for which the remainder of this config is valid. This
  // pointer will remain valid for the duration of the streaming session.
  raw_ptr<const openscreen::cast::ReceiverSession> session;

  // Information detailing the audio stream. Will be populated iff the streaming
  // session has audio.
  std::optional<AudioStreamInfo> audio_stream_info;

  // Information detailing the video stream. Will be populated iff the streaming
  // session has video.
  std::optional<VideoStreamInfo> video_stream_info;

  // Whether or not this streaming session is associated with remoting (as
  // opposed to mirroring).
  bool is_remoting;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_COMMON_STREAMING_INITIALIZATION_INFO_H_
