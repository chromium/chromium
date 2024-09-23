// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/browser/test_support.h"

#include "base/base64.h"
#include "components/tpcd/metadata/common/proto/metadata.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tpcd::metadata {
std::string MakeBase64EncodedMetadata(const Metadata& metadata) {
  std::string compressed;
  compression::GzipCompress(metadata.SerializeAsString(), &compressed);

  return base::Base64Encode(compressed);
}

uint32_t DeterministicGenerator::Generate() const {
  return rand_;
}
}  // namespace tpcd::metadata
