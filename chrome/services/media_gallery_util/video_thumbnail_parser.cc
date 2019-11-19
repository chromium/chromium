// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/video_thumbnail_parser.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/services/media_gallery_util/ipc_data_source.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_codecs.h"
#include "media/base/video_thumbnail_decoder.h"
#include "media/filters/android/video_frame_extractor.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/media_buildflags.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

namespace {

// Return the video frame back to browser process. A valid |config| is
// needed for deserialization.
void OnSoftwareVideoFrameDecoded(
    std::unique_ptr<media::VideoThumbnailDecoder>,
    MediaParser::ExtractVideoFrameCallback video_frame_callback,
    const media::VideoDecoderConfig& config,
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(video_frame_callback);

  if (!frame) {
    std::move(video_frame_callback)
        .Run(false, chrome::mojom::VideoFrameData::New(), base::nullopt);
    return;
  }

  std::move(video_frame_callback)
      .Run(true,
           chrome::mojom::VideoFrameData::NewDecodedFrame(
               media::MojoSharedBufferVideoFrame::CreateFromYUVFrame(*frame)),
           config);
}

void OnEncodedVideoFrameExtracted(
    std::unique_ptr<media::VideoFrameExtractor> video_frame_extractor,
    MediaParser::ExtractVideoFrameCallback video_frame_callback,
    bool success,
    std::vector<uint8_t> data,
    const media::VideoDecoderConfig& config) {
  if (!success || data.empty()) {
    std::move(video_frame_callback)
        .Run(false, chrome::mojom::VideoFrameData::New(), base::nullopt);
    return;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS) && \
    !BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  // H264 currently needs to be decoded in GPU process when no software decoder
  // is provided.
  if (config.codec() == media::VideoCodec::kCodecH264) {
    std::move(video_frame_callback)
        .Run(success,
             chrome::mojom::VideoFrameData::NewEncodedData(std::move(data)),
             config);
    return;
  }
#endif

  if (config.codec() != media::VideoCodec::kCodecVP8 &&
      config.codec() != media::VideoCodec::kCodecVP9) {
    std::move(video_frame_callback)
        .Run(false, chrome::mojom::VideoFrameData::New(), base::nullopt);
    return;
  }

  // Decode with libvpx for vp8, vp9.
  auto thumbnail_decoder = std::make_unique<media::VideoThumbnailDecoder>(
      std::make_unique<media::VpxVideoDecoder>(), config, std::move(data));

  thumbnail_decoder->Start(
      base::BindOnce(&OnSoftwareVideoFrameDecoded, std::move(thumbnail_decoder),
                     std::move(video_frame_callback), config));
}

void ExtractVideoFrameOnMediaThread(
    media::DataSource* data_source,
    MediaParser::ExtractVideoFrameCallback video_frame_callback) {
  auto extractor = std::make_unique<media::VideoFrameExtractor>(data_source);
  extractor->Start(base::BindOnce(&OnEncodedVideoFrameExtracted,
                                  std::move(extractor),
                                  std::move(video_frame_callback)));
}

}  // namespace

VideoThumbnailParser::VideoThumbnailParser(
    std::unique_ptr<media::DataSource> source)
    : data_source_(std::move(source)),
      media_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock()})) {}

VideoThumbnailParser::~VideoThumbnailParser() = default;

void VideoThumbnailParser::Start(
    MediaParser::ExtractVideoFrameCallback video_frame_callback) {
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExtractVideoFrameOnMediaThread, data_source_.get(),
          media::BindToCurrentLoop(std::move(video_frame_callback))));
}
