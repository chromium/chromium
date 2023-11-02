// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/time/time.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

class MediaParser : public chrome::mojom::MediaParser {
 public:
  MediaParser();

  MediaParser(const MediaParser&) = delete;
  MediaParser& operator=(const MediaParser&) = delete;

  ~MediaParser() override;

 private:
  // chrome::mojom::MediaParser:
  void ParseMediaMetadata(
      const std::string& mime_type,
      int64_t total_size,
      bool get_attached_images,
      mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
      ParseMediaMetadataCallback callback) override;
  void CheckMediaFile(base::TimeDelta decode_time,
                      base::File file,
                      CheckMediaFileCallback callback) override;
  void GetCpuInfo(GetCpuInfoCallback callback) override;
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_H_
