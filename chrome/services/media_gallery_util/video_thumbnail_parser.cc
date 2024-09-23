// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/video_thumbnail_parser.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/services/media_gallery_util/ipc_data_source.h"
#include "media/base/media_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_codecs.h"
#include "media/base/video_thumbnail_decoder.h"
#include "media/filters/android/video_frame_extractor.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_AV1_DECODER)
#include "media/filters/dav1d_video_decoder.h"
#endif

namespace {

// Return the video frame back to browser process. A valid |config| is
// needed for deserialization.
void OnSoftwareVideoFrameDecoded(
    std::unique_ptr<media::MediaLog>,
    std::unique_ptr<media::VideoThumbnailDecoder>,
    MediaParser::ExtractVideoFrameCallback video_frame_callback,
    const media::VideoDecoderConfig& config,
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(video_frame_callback);

  if (!frame) {
    std::move(video_frame_callback).Run(nullptr);
    return;
  }

  std::move(video_frame_callback)
      .Run(chrome::mojom::ExtractVideoFrameResult::New(
          chrome::mojom::VideoFrameData::NewDecodedFrame(std::move(frame)),
          config));
}

void OnEncodedVideoFrameExtracted(
    std::unique_ptr<media::VideoFrameExtractor> video_frame_extractor,
    MediaParser::ExtractVideoFrameCallback video_frame_callback,
    bool success,
    std::vector<uint8_t> data,
    const media::VideoDecoderConfig& config) {
  if (!success || data.empty()) {
    std::move(video_frame_callback).Run(nullptr);
    return;
  }

  std::unique_ptr<media::MediaLog> log;
  std::unique_ptr<media::VideoDecoder> software_decoder;
  if (media::IsBuiltInVideoCodec(config.codec())) {
    switch (config.codec()) {
      case media::VideoCodec::kH264:
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
        log = std::make_unique<media::NullMediaLog>();
        software_decoder =
            std::make_unique<media::FFmpegVideoDecoder>(log.get());
        break;
#else
        // IsBuiltInVideoCodec(H264) should never return true if
        // ENABLE_FFMPEG_VIDEO_DECODERS is false.
        NOTREACHED();
#endif
      case media::VideoCodec::kVP8:
      case media::VideoCodec::kVP9:
#if BUILDFLAG(ENABLE_LIBVPX)
        software_decoder = std::make_unique<media::VpxVideoDecoder>();
        break;
#else
        // IsBuiltInVideoCodec(VP8|VP9) should never return true if
        // ENABLE_FFMPEG_VIDEO_DECODERS is false.
        NOTREACHED();
#endif
      case media::VideoCodec::kAV1:
#if BUILDFLAG(ENABLE_AV1_DECODER)
        software_decoder = std::make_unique<media::Dav1dVideoDecoder>(
            std::make_unique<media::NullMediaLog>());
        break;
#else
        // IsBuiltInVideoCodec(AV1) should never return true if
        // ENABLE_FFMPEG_VIDEO_DECODERS is false.
        NOTREACHED();
#endif

      default:
        std::move(video_frame_callback).Run(nullptr);
        return;
    }
  } else if (config.codec() == media::VideoCodec::kH264 ||
             config.codec() == media::VideoCodec::kHEVC) {
    std::move(video_frame_callback)
        .Run(chrome::mojom::ExtractVideoFrameResult::New(
            chrome::mojom::VideoFrameData::NewEncodedData(std::move(data)),
            config));
    return;
  } else {
    std::move(video_frame_callback).Run(nullptr);
    return;
  }

  auto thumbnail_decoder = std::make_unique<media::VideoThumbnailDecoder>(
      std::move(software_decoder), config, std::move(data));

  auto* const thumbnail_decoder_ptr = thumbnail_decoder.get();
  thumbnail_decoder_ptr->Start(base::BindOnce(
      &OnSoftwareVideoFrameDecoded, std::move(log),
      std::move(thumbnail_decoder), std::move(video_frame_callback), config));
}

void ExtractVideoFrameOnMediaThread(
    media::DataSource* data_source,
    MediaParser::ExtractVideoFrameCallback video_frame_callback) {
  auto extractor = std::make_unique<media::VideoFrameExtractor>(data_source);
  auto* const extractor_ptr = extractor.get();
  extractor_ptr->Start(base::BindOnce(&OnEncodedVideoFrameExtracted,
                                      std::move(extractor),
                                      std::move(video_frame_callback)));
}

}  // namespace

VideoThumbnailParser::VideoThumbnailParser(
    std::unique_ptr<media::DataSource> source)
    : data_source_(std::move(source)),
      media_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

VideoThumbnailParser::~VideoThumbnailParser() = default;

void VideoThumbnailParser::Start(
    MediaParser::ExtractVideoFrameCallback video_frame_callback) {
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExtractVideoFrameOnMediaThread, data_source_.get(),
          base::BindPostTaskToCurrentDefault(std::move(video_frame_callback))));
}
