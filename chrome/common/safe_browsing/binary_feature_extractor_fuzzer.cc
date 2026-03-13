// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static safe_browsing::BinaryFeatureExtractor* extractor =
      new safe_browsing::BinaryFeatureExtractor();

  google::protobuf::RepeatedPtrField<std::string> signed_data;
  safe_browsing::ClientDownloadRequest_ImageHeaders image_headers;
  extractor->ExtractImageFeaturesFromData(
      data, safe_browsing::BinaryFeatureExtractor::kDefaultOptions,
      &image_headers, &signed_data);
  return 0;
}
