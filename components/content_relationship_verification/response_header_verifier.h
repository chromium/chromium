// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_RESPONSE_HEADER_VERIFIER_H_
#define COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_RESPONSE_HEADER_VERIFIER_H_

#include <string>

namespace content_relationship_verification {

class ResponseHeaderVerifier {
 public:
  // Verify if the provided |package_name| is verified via the embedder
  // ancestor header.
  static bool Verify(const std::string& package_name,
                     const std::string& embedder_ancestors_header_value);
};

extern const char kEmbedderAncestorHeader[];
}  // namespace content_relationship_verification

#endif  // COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_RESPONSE_HEADER_VERIFIER_H_
