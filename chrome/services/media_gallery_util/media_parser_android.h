// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_ANDROID_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "chrome/services/media_gallery_util/media_parser.h"

namespace media {
class DataSource;
}  // namespace media

// The media parser on Android that provides video thumbnail generation utility.
class MediaParserAndroid : public MediaParser {
 public:
  MediaParserAndroid(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~MediaParserAndroid() override;

  // MediaParser implementation.
  void ExtractVideoFrame(
      const std::string& mime_type,
      uint32_t total_size,
      chrome::mojom::MediaDataSourcePtr media_data_source,
      ExtractVideoFrameCallback video_frame_callback) override;

 private:
  // The task runner to do blocking IO. The utility thread cannot be blocked.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  std::unique_ptr<media::DataSource> data_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaParserAndroid);
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_ANDROID_H_
