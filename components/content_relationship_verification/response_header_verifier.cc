// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_relationship_verification/response_header_verifier.h"

#include <stdio.h>

#include "base/containers/contains.h"
#include "base/strings/string_split.h"

namespace {
const char kNormalizedHeaderDelimiter[] = ",";
}  // namespace

namespace content_relationship_verification {

// Valid header values:
//   - '*': All Android packages can displays this website
//   - 'none': No permissions given to any Android App
//   -  <List of package names>: package names with access to the web content.
const char kEmbedderAncestorHeader[] = "X-Embedder-Ancestors";

// TODO(crbug.com/40243409): Also support fingerprints.
ResponseHeaderVerificationResult ResponseHeaderVerifier::Verify(
    const std::string& package_name,
    const std::string& embedder_ancestors_header_value) {
  // No embedder-ancestor-header defaults to verified.
  if (embedder_ancestors_header_value.empty()) {
    // TODO(crbug.com/40243409): Set to false if undecided content should be
    // treated like explicitly unconsenting content.
    return ResponseHeaderVerificationResult::kMissing;
  }

  if (embedder_ancestors_header_value == "*") {
    return ResponseHeaderVerificationResult::kAllow;
  }
  if (embedder_ancestors_header_value == "none") {
    return ResponseHeaderVerificationResult::kDisallow;
  }

  std::vector<std::string> allowed_package_names =
      SplitString(embedder_ancestors_header_value, kNormalizedHeaderDelimiter,
                  base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (base::Contains(allowed_package_names, package_name)) {
    return ResponseHeaderVerificationResult::kAllow;
  }

  return ResponseHeaderVerificationResult::kDisallow;
}

}  // namespace content_relationship_verification
