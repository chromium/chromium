// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_PARSER_TEST_HELPER_H_
#define COMPONENTS_TPCD_METADATA_PARSER_TEST_HELPER_H_

#include "components/tpcd/metadata/metadata.pb.h"

namespace tpcd::metadata {
std::string MakeBase64EncodedMetadata(const Metadata& metadata);
}  // namespace tpcd::metadata
#endif  // COMPONENTS_TPCD_METADATA_PARSER_TEST_HELPER_H_
