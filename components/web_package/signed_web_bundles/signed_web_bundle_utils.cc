// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"

#include <cstdint>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"

namespace web_package {

namespace {

void AddItemToPayload(std::vector<uint8_t>& payload,
                      base::span<const uint8_t> item) {
  // Each item that is part of the payload is prefixed with its length encoded
  // as a 64 bit unsigned integer.
  std::array<uint8_t, sizeof(uint64_t)> length;
  auto writer = base::SpanWriter(base::span(length));
  CHECK(writer.WriteU64BigEndian(item.size()));

  base::Extend(payload, base::span(length));
  base::Extend(payload, item);
}

}  // namespace

std::vector<uint8_t> CreateSignaturePayload(
    const SignedWebBundleSignatureData& data) {
  std::vector<uint8_t> payload;
  AddItemToPayload(payload, data.unsigned_web_bundle_hash);
  AddItemToPayload(payload, data.integrity_block_cbor);
  AddItemToPayload(payload, data.attributes_cbor);

  return payload;
}

}  // namespace web_package
