// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_XZ_FILE_EXTRACTOR_H_
#define CHROME_SERVICES_FILE_UTIL_XZ_FILE_EXTRACTOR_H_

#include "chrome/services/file_util/public/mojom/xz_file_extractor.mojom.h"

class XzFileExtractor : public chrome::mojom::XzFileExtractor {
 public:
  XzFileExtractor();
  ~XzFileExtractor() override;
  XzFileExtractor(const XzFileExtractor&) = delete;
  XzFileExtractor& operator=(const XzFileExtractor&) = delete;

  // chrome::mojom::XzFileExtractor:
  void Extract(mojo::ScopedDataPipeConsumerHandle xz_stream,
               mojo::ScopedDataPipeProducerHandle extracted_stream,
               ExtractCallback callback) override;
};

#endif  // CHROME_SERVICES_FILE_UTIL_XZ_FILE_EXTRACTOR_H_
