// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_XZ_FILE_EXTRACTOR_H_
#define CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_XZ_FILE_EXTRACTOR_H_

#include "base/files/file.h"
#include "chrome/services/file_util/public/mojom/single_file_extractor.mojom.h"

class SingleFileTarXzFileExtractor : public chrome::mojom::SingleFileExtractor {
 public:
  SingleFileTarXzFileExtractor();
  ~SingleFileTarXzFileExtractor() override;
  SingleFileTarXzFileExtractor(const SingleFileTarXzFileExtractor&) = delete;
  SingleFileTarXzFileExtractor& operator=(const SingleFileTarXzFileExtractor&) =
      delete;

  // chrome::mojom::SingleFileExtractor implementation.
  void Extract(
      base::File src_file,
      base::File dst_file,
      mojo::PendingRemote<chrome::mojom::SingleFileExtractorListener> listener,
      ExtractCallback callback) override;
};

#endif  // CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_XZ_FILE_EXTRACTOR_H_
