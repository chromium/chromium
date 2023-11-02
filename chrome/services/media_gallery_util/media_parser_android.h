// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_ANDROID_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_ANDROID_H_

#include <memory>

#include "chrome/services/media_gallery_util/media_parser.h"

// The media parser on Android that provides video thumbnail generation utility.
class MediaParserAndroid : public MediaParser {
 public:
  MediaParserAndroid();

  MediaParserAndroid(const MediaParserAndroid&) = delete;
  MediaParserAndroid& operator=(const MediaParserAndroid&) = delete;

  ~MediaParserAndroid() override;

  // MediaParser implementation.
  void ExtractVideoFrame(
      const std::string& mime_type,
      uint32_t total_size,
      mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
      ExtractVideoFrameCallback video_frame_callback) override;
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_ANDROID_H_
