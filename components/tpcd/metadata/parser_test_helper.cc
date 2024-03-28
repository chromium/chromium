// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/parser_test_helper.h"

#include "base/base64.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tpcd::metadata {
std::string MakeBase64EncodedMetadata(const Metadata& metadata) {
  std::string compressed;
  compression::GzipCompress(metadata.SerializeAsString(), &compressed);

  return base::Base64Encode(compressed);
}

}  // namespace tpcd::metadata
