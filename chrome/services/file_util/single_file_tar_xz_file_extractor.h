// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_XZ_FILE_EXTRACTOR_H_
#define CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_XZ_FILE_EXTRACTOR_H_

#include "base/files/file.h"
#include "chrome/services/file_util/public/mojom/single_file_tar_xz_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class SingleFileTarXzFileExtractor
    : public chrome::mojom::SingleFileTarXzFileExtractor {
 public:
  SingleFileTarXzFileExtractor();
  ~SingleFileTarXzFileExtractor() override;
  SingleFileTarXzFileExtractor(const SingleFileTarXzFileExtractor&) = delete;
  SingleFileTarXzFileExtractor& operator=(const SingleFileTarXzFileExtractor&) =
      delete;

  // chrome::mojom::SingleFileTarXzFileExtractor implementation.
  void Extract(
      base::File src_file,
      base::File dst_file,
      mojo::PendingRemote<chrome::mojom::SingleFileTarXzFileExtractorListener>
          listener,
      ExtractCallback callback) override;
};

#endif  // CHROME_SERVICES_FILE_UTIL_SINGLE_FILE_TAR_XZ_FILE_EXTRACTOR_H_
