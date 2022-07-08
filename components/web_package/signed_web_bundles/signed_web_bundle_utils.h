// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_UTILS_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_UTILS_H_

#include <vector>

#include "base/containers/span.h"

namespace web_package {

// Helper function to construct and correctly encode the payload from the
// unsigned web bundle's hash, the integrity block, and the attributes of the
// signature stack entry. The payload can then be used to verify or calculate
// the signed web bundle's signature.
std::vector<uint8_t> CreateSignaturePayload(
    base::span<const uint8_t> unsigned_bundle_hash,
    base::span<const uint8_t> integrity_block,
    base::span<const uint8_t> signature_stack_entry_attributes);

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_UTILS_H_
