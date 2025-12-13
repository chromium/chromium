// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_DEMUXER_STREAM_READER_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_DEMUXER_STREAM_READER_H_

#include <array>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/starboard/media/media/drm_util.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/renderer_client.h"

namespace chromecast {

namespace metrics {
class CastMetricsHelper;
}  // namespace metrics

namespace media {

// Receives buffers from one or more DemuxerStreams, calling handle_buffer_cb
// when a buffer is ready to be processed. Audio buffers are processed via
// `convert_audio_fn` before being passed to starboard, e.g. to convert PCM
// data to S16.
//
// This class ensures that encrypted buffers are not handled until the
// relevant DRM key is available to the CDM.
//
// This class must only be used on one sequence.
class DemuxerStreamReader {
 public:
  // Note that this contains both a StarboardSampleInfo and a
  // scoped_refptr<DecoderBuffer>. This is so that the lifetime of the
  // underlying buffer data (stored in the scoped_refptr) can be managed
  // properly.
  //
  // sample_info may contain pointers (to DRM-related info) that become
  // invalid after the callback returns.
  using HandleBufferCb = base::RepeatingCallback<void(
      int seek_ticket,
      StarboardSampleInfo sample_info,
      scoped_refptr<::media::DecoderBuffer> buffer)>;

  using HandleEosCb =
      base::RepeatingCallback<void(int seek_ticket, StarboardMediaType type)>;

  DemuxerStreamReader(
      ::media::DemuxerStream* audio_stream,
      ::media::DemuxerStream* video_stream,
      std::optional<StarboardAudioSampleInfo> audio_sample_info,
      std::optional<StarboardVideoSampleInfo> video_sample_info,
      HandleBufferCb handle_buffer_cb,
      HandleEosCb handle_eos_cb,
      ::media::RendererClient* client,
      chromecast::metrics::CastMetricsHelper* cast_metrics_helper);

  ~DemuxerStreamReader();

  // Requests that a buffer of type `type` be read from the relevant
  // DemuxerStream. `seek_ticket` and `type` will be passed to the
  // HandleBufferCb along with the buffer.
  void ReadBuffer(int seek_ticket, StarboardMediaType type);

 private:
  using ConvertAudioFn =
      base::RepeatingCallback<scoped_refptr<::media::DecoderBuffer>(
          scoped_refptr<::media::DecoderBuffer>)>;

  // Callback called by a DemuxerStream.
  void OnReadBuffer(StarboardMediaType type,
                    int seek_ticket,
                    ::media::DemuxerStream::Status status,
                    std::vector<scoped_refptr<::media::DecoderBuffer>> buffers);

  // Handles a non-OK status from a DemuxerStream of type `type`. `status` must
  // NOT be OK.
  void HandleNonOkDemuxerStatus(::media::DemuxerStream::Status status,
                                StarboardMediaType type,
                                int seek_ticket);

  // Waits for a DRM key to be available to the CDM. Once the key is available,
  // the buffer will be pushed to starboard via handle_buffer_cb_.
  void WaitForKey(DrmInfoWrapper drm_info,
                  StarboardSampleInfo sample_info,
                  scoped_refptr<::media::DecoderBuffer> buffer,
                  int seek_ticket);

  // Runs a pending callback now that a DRM key is available.
  void RunPendingDrmKeyCallback(int64_t token);

  // Updates the audio config to match the current config from audio_stream_. If
  // the change is unsupported (e.g. if the codec changed), returns false;
  // returns true if the config change is supported.
  bool UpdateAudioConfig();

  // Updates the video config to match the current config from video_stream_. If
  // the change is unsupported (e.g. if the codec changed), returns false;
  // returns true if the config change is supported.
  bool UpdateVideoConfig();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<chromecast::metrics::CastMetricsHelper> cast_metrics_helper_ =
      nullptr;
  ConvertAudioFn convert_audio_fn_;
  HandleBufferCb handle_buffer_cb_;
  HandleEosCb handle_eos_cb_;
  raw_ptr<::media::RendererClient> client_;
  raw_ptr<::media::DemuxerStream> audio_stream_ = nullptr;
  raw_ptr<::media::DemuxerStream> video_stream_ = nullptr;

  // Tracks whether there is a pending audio/video read. Indices correspond to
  // StarboardMediaType.
  std::array<bool, 2> pending_read_ = {false, false};

  // StarboardAudioSampleInfo contains a const void* audio_specific_config. That
  // field can point to the extra data of this config, so we should ensure that
  // the ptr remains valid until the next buffer is read.
  ::media::AudioDecoderConfig chromium_audio_config_;
  std::optional<StarboardAudioSampleInfo> audio_sample_info_;
  std::optional<StarboardVideoSampleInfo> video_sample_info_;
  bool first_video_frame_ = true;
  // Maps from opaque token to a callback that should be run when the DRM key
  // represented by that token is available.
  base::flat_map<int64_t, base::OnceClosure> token_to_drm_key_cb_;

  // This should be destructed first, to invalidate any weak ptrs.
  base::WeakPtrFactory<DemuxerStreamReader> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_DEMUXER_STREAM_READER_H_
