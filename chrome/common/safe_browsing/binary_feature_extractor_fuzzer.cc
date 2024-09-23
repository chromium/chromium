// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "components/safe_browsing/core/common/proto/csd.pb.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static safe_browsing::BinaryFeatureExtractor* extractor =
      new safe_browsing::BinaryFeatureExtractor();

  google::protobuf::RepeatedPtrField<std::string> signed_data;
  safe_browsing::ClientDownloadRequest_ImageHeaders image_headers;
  // SAFETY: Libfuzzer guarantees `data` has size `size`.
  extractor->ExtractImageFeaturesFromData(
      UNSAFE_BUFFERS(base::span(data, size)),
      safe_browsing::BinaryFeatureExtractor::kDefaultOptions, &image_headers,
      &signed_data);
  return 0;
}
