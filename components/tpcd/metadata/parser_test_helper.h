// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_PARSER_TEST_HELPER_H_
#define COMPONENTS_TPCD_METADATA_PARSER_TEST_HELPER_H_

#include <utility>
#include <vector>

#include "components/tpcd/metadata/metadata.pb.h"
#include "components/tpcd/metadata/parser.h"

namespace tpcd::metadata {

using MetadataPair = std::pair<std::string, std::string>;

void AddEntryToMetadata(Metadata& metadata,
                        const std::string& primary_pattern_spec,
                        const std::string& secondary_pattern_spec,
                        const std::string& source = Parser::kSourceTest);

std::string MakeBase64EncodedMetadata(const Metadata& metadata);

}  // namespace tpcd::metadata

#endif  // COMPONENTS_TPCD_METADATA_PARSER_TEST_HELPER_H_
