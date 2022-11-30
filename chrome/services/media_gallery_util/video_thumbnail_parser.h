// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_VIDEO_THUMBNAIL_PARSER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_VIDEO_THUMBNAIL_PARSER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/media_gallery_util/media_parser.h"

namespace media {
class DataSource;
}  // namespace media

// Parses a video frame. This object is created on utility process main thread,
// and will perform actual parsing on a media thread.
class VideoThumbnailParser {
 public:
  explicit VideoThumbnailParser(std::unique_ptr<media::DataSource> source);

  VideoThumbnailParser(const VideoThumbnailParser&) = delete;
  VideoThumbnailParser& operator=(const VideoThumbnailParser&) = delete;

  ~VideoThumbnailParser();

  void Start(MediaParser::ExtractVideoFrameCallback video_frame_callback);

 private:
  // The data source that provides video data. Created and destroyed on utility
  // main thread because it's binded to mojo object. Must be used on
  // |media_task_runner_|.
  std::unique_ptr<media::DataSource> data_source_;

  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_VIDEO_THUMBNAIL_PARSER_H_
