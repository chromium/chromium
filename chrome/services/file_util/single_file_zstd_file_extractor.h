// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_ZSTD_FILE_EXTRACTOR_H_
#define CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_ZSTD_FILE_EXTRACTOR_H_

#include "base/files/file.h"
#include "chrome/services/file_util/public/mojom/single_file_extractor.mojom.h"

// Extracts a single-stream Zstandard (.zst) archive in the FileUtilService
// utility process.
class SingleFileZstdFileExtractor : public chrome::mojom::SingleFileExtractor {
 public:
  SingleFileZstdFileExtractor();
  SingleFileZstdFileExtractor(const SingleFileZstdFileExtractor&) = delete;
  SingleFileZstdFileExtractor& operator=(const SingleFileZstdFileExtractor&) =
      delete;
  ~SingleFileZstdFileExtractor() override;

  // chrome::mojom::SingleFileExtractor implementation.
  void Extract(
      base::File src_file,
      base::File dst_file,
      mojo::PendingRemote<chrome::mojom::SingleFileExtractorListener> listener,
      ExtractCallback callback) override;
};

#endif  // CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_ZSTD_FILE_EXTRACTOR_H_
