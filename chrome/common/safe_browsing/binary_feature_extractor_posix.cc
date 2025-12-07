// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a stub for the code signing utilities on Mac and Linux.
// It should eventually be replaced with a real implementation.

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"

namespace safe_browsing {

void BinaryFeatureExtractor::CheckSignature(
    const base::FilePath& file_path,
    ClientDownloadRequest_SignatureInfo* signature_info) {}

#if !BUILDFLAG(IS_MAC)
bool BinaryFeatureExtractor::ExtractImageFeaturesFromData(
    base::span<const uint8_t> data,
    ExtractHeadersOption options,
    ClientDownloadRequest_ImageHeaders* image_headers,
    google::protobuf::RepeatedPtrField<std::string>* signed_data) {
  return false;
}
#endif

}  // namespace safe_browsing
