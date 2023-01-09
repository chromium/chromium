// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_METADATA_PARSER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_METADATA_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

namespace base {
class Thread;
}

namespace media {
class DataSource;
}

// This class takes a MIME type and data source and parses its metadata. It
// handles audio, video, and images. It delegates its operations to FFMPEG.
// This class lives and operates on the utility thread of the utility process
// so we sandbox potentially dangerous operations on user-provided data.
class MediaMetadataParser {
 public:
  using MetadataCallback = base::OnceCallback<void(
      chrome::mojom::MediaMetadataPtr metadata,
      const std::vector<metadata::AttachedImage>& attached_images)>;

  MediaMetadataParser(std::unique_ptr<media::DataSource> source,
                      const std::string& mime_type,
                      bool get_attached_images);

  MediaMetadataParser(const MediaMetadataParser&) = delete;
  MediaMetadataParser& operator=(const MediaMetadataParser&) = delete;

  ~MediaMetadataParser();

  // |callback| is called on same message loop.
  void Start(MetadataCallback callback);

 private:
  // Only accessed on |media_thread_| from this class.
  std::unique_ptr<media::DataSource> source_;

  const std::string mime_type_;

  bool get_attached_images_;

  // Thread that blocking media parsing operations run on while the main thread
  // handles messages from the browser process.
  // TODO(tommycli): Replace with a reference to a WorkerPool if we ever use
  // this class in batch mode.
  std::unique_ptr<base::Thread> media_thread_;
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_METADATA_PARSER_H_
