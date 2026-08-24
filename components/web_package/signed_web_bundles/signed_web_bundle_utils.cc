// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"

#include <cstdint>
#include <vector>

#include "components/web_package/signed_web_bundles/rust/signed_web_bundles_rust.h"

namespace web_package {

std::vector<uint8_t> CreateSignaturePayload(
    const SignedWebBundleSignatureData& data) {
  auto payload_rust = signed_web_bundles::rust::create_signature_payload(
      data.unsigned_web_bundle_hash, data.integrity_block_cbor,
      data.attributes_cbor);
  return std::vector<uint8_t>(payload_rust.begin(), payload_rust.end());
}

}  // namespace web_package
