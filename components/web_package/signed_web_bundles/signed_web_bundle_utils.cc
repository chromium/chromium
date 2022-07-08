// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"

#include "base/big_endian.h"

namespace web_package {

namespace {

void AddItemToPayload(std::vector<uint8_t>& payload,
                      base::span<const uint8_t> item) {
  // Each item that is part of the payload is prefixed with its length encoded
  // as a 64 bit unsigned integer.
  std::array<char, sizeof(uint64_t)> length;
  base::BigEndianWriter writer(length.data(), length.size());
  CHECK(writer.WriteU64(item.size()));

  payload.insert(payload.end(), length.begin(), length.end());
  payload.insert(payload.end(), item.begin(), item.end());
}

}  // namespace

std::vector<uint8_t> CreateSignaturePayload(
    base::span<const uint8_t> unsigned_bundle_hash,
    base::span<const uint8_t> integrity_block,
    base::span<const uint8_t> signature_stack_entry_attributes) {
  std::vector<uint8_t> payload;
  AddItemToPayload(payload, unsigned_bundle_hash);
  AddItemToPayload(payload, integrity_block);
  AddItemToPayload(payload, signature_stack_entry_attributes);

  return payload;
}

}  // namespace web_package
