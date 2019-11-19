// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_parser_android.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "chrome/services/media_gallery_util/ipc_data_source.h"
#include "chrome/services/media_gallery_util/video_thumbnail_parser.h"

namespace {

void OnVideoFrameExtracted(
    std::unique_ptr<VideoThumbnailParser>,
    MediaParser::ExtractVideoFrameCallback video_frame_callback,
    bool success,
    chrome::mojom::VideoFrameDataPtr frame_data,
    const base::Optional<media::VideoDecoderConfig>& config) {
  std::move(video_frame_callback).Run(success, std::move(frame_data), config);
}

}  // namespace

MediaParserAndroid::MediaParserAndroid() = default;

MediaParserAndroid::~MediaParserAndroid() = default;

void MediaParserAndroid::ExtractVideoFrame(
    const std::string& mime_type,
    uint32_t total_size,
    mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
    MediaParser::ExtractVideoFrameCallback video_frame_callback) {
  auto data_source = std::make_unique<IPCDataSource>(
      std::move(media_data_source), static_cast<int64_t>(total_size));

  // Leak |parser| on utility main thread, because |data_source| lives on main
  // thread and is used on another thread as raw pointer. Leaked |parser| will
  // be deleted when utility process dies or |OnVideoFrameExtracted| callback
  // is called.
  auto parser = std::make_unique<VideoThumbnailParser>(std::move(data_source));
  parser->Start(base::BindOnce(&OnVideoFrameExtracted, std::move(parser),
                               std::move(video_frame_callback)));
}
