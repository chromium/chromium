// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/digital_asset_links/response_header_verifier.h"

#include <stdio.h>

#include "base/containers/contains.h"
#include "base/strings/string_split.h"

namespace {
const char kNormalizedHeaderDelimiter[] = ",";
}  // namespace

namespace digital_asset_links {

const char kEmbedderAncestorHeader[] = "X-Embedder-Ancestors";

// TODO(crbug.com/1376958): Also support fingerprints.
bool ResponseHeaderVerifier::Verify(
    const std::string& package_name,
    const std::string& embedder_ancestors_header_value) {
  // No embedder-ancestor-header defaults to verified.
  if (embedder_ancestors_header_value.empty()) {
    // TODO(crbug.com/1376958): Set to false if undecided content should be
    // treated like explicitly unconsenting content.
    return true;
  }

  if (embedder_ancestors_header_value == "*") {
    return true;
  }
  if (embedder_ancestors_header_value == "none") {
    return false;
  }

  std::vector<std::string> allowed_package_names =
      SplitString(embedder_ancestors_header_value, kNormalizedHeaderDelimiter,
                  base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (base::Contains(allowed_package_names, package_name)) {
    return true;
  }

  return false;
}

}  // namespace digital_asset_links
