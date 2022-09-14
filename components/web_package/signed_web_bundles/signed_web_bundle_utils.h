// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_UTILS_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_UTILS_H_

#include <vector>

#include "base/containers/span.h"

namespace web_package {

struct SignedWebBundleSignatureData {
  base::span<const uint8_t> unsigned_web_bundle_hash;
  base::span<const uint8_t> integrity_block_cbor;
  base::span<const uint8_t> attributes_cbor;
};

// Utility function to construct and correctly encode the message for signature
// creation and verification of a signature stack entry's signature of a Signed
// Web Bundle. The arguments are provided in a struct, so that callers of this
// function are less likely to accidentally use the wrong order of arguments
// when calling this function, which could lead to signatures being created or
// verified incorrectly.
std::vector<uint8_t> CreateSignaturePayload(
    const SignedWebBundleSignatureData& data);

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_UTILS_H_
