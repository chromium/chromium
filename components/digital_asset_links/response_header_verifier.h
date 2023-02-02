// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DIGITAL_ASSET_LINKS_RESPONSE_HEADER_VERIFIER_H_
#define COMPONENTS_DIGITAL_ASSET_LINKS_RESPONSE_HEADER_VERIFIER_H_

#include <string>

namespace digital_asset_links {

class ResponseHeaderVerifier {
 public:
  // Verify if the provided |package_name| is verified via the embedder
  // ancestor header.
  static bool Verify(const std::string& package_name,
                     const std::string& embedder_ancestors_header_value);
};

extern const char kEmbedderAncestorHeader[];
}  // namespace digital_asset_links

#endif  // COMPONENTS_DIGITAL_ASSET_LINKS_RESPONSE_HEADER_VERIFIER_H_
