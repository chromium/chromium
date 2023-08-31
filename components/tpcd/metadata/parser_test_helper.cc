// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/parser_test_helper.h"

#include "base/base64.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tpcd::metadata {

Metadata MakeMetadataProtoFromVectorOfPair(
    const std::vector<MetadataPair>& metadata_pairs) {
  Metadata metadata;
  for (const MetadataPair& metadata_pair : metadata_pairs) {
    MetadataEntry* me = metadata.add_metadata_entries();
    me->set_primary_pattern_spec(metadata_pair.first);
    me->set_secondary_pattern_spec(metadata_pair.second);
  }
  return metadata;
}

std::string MakeBase64EncodedMetadata(
    const std::vector<MetadataPair>& metadata_pairs) {
  std::string compressed;
  compression::GzipCompress(
      MakeMetadataProtoFromVectorOfPair(metadata_pairs).SerializeAsString(),
      &compressed);

  std::string encoded;
  base::Base64Encode(compressed, &encoded);

  return encoded;
}

}  // namespace tpcd::metadata
