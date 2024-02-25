// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/parser_test_helper.h"

#include "base/base64.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "components/tpcd/metadata/parser.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tpcd::metadata {

void AddEntryToMetadata(Metadata& metadata,
                        const std::string& primary_pattern_spec,
                        const std::string& secondary_pattern_spec,
                        const std::string& source) {
  MetadataEntry* me = metadata.add_metadata_entries();
  me->set_primary_pattern_spec(primary_pattern_spec);
  me->set_secondary_pattern_spec(secondary_pattern_spec);
  me->set_source(source);
}

std::string MakeBase64EncodedMetadata(const Metadata& metadata) {
  std::string compressed;
  compression::GzipCompress(metadata.SerializeAsString(), &compressed);

  return base::Base64Encode(compressed);
}

}  // namespace tpcd::metadata
