// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_parser_android.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/services/media_gallery_util/ipc_data_source.h"
#include "chrome/services/media_gallery_util/video_thumbnail_parser.h"

namespace {

void OnVideoFrameExtracted(
    std::unique_ptr<VideoThumbnailParser>,
    MediaParser::ExtractVideoFrameCallback video_frame_callback,
    chrome::mojom::ExtractVideoFrameResultPtr result) {
  std::move(video_frame_callback).Run(std::move(result));
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
  auto* const parser_ptr = parser.get();
  parser_ptr->Start(base::BindOnce(&OnVideoFrameExtracted, std::move(parser),
                                   std::move(video_frame_callback)));
}
