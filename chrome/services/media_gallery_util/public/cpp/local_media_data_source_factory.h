// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_LOCAL_MEDIA_DATA_SOURCE_FACTORY_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_LOCAL_MEDIA_DATA_SOURCE_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/media_gallery_util/public/cpp/safe_media_metadata_parser.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom-forward.h"

namespace base {
class FilePath;
}  // namespace base

// Provides local media data in the browser process and send it to media gallery
// util service to parse media metadata safely in an utility process.
class LocalMediaDataSourceFactory
    : public SafeMediaMetadataParser::MediaDataSourceFactory {
 public:
  LocalMediaDataSourceFactory(
      const base::FilePath& file_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);

  LocalMediaDataSourceFactory(const LocalMediaDataSourceFactory&) = delete;
  LocalMediaDataSourceFactory& operator=(const LocalMediaDataSourceFactory&) =
      delete;

  ~LocalMediaDataSourceFactory() override;

  // SafeMediaMetadataParser::MediaDataSourceFactory implementation.
  std::unique_ptr<chrome::mojom::MediaDataSource> CreateMediaDataSource(
      mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
      MediaDataCallback media_data_callback) override;

 private:
  // Local downloaded media file path. This is user-defined input.
  base::FilePath file_path_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_LOCAL_MEDIA_DATA_SOURCE_FACTORY_H_
