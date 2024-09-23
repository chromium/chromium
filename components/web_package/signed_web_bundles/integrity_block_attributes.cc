// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"

namespace web_package {

IntegrityBlockAttributes::IntegrityBlockAttributes(std::string web_bundle_id,
                                                   std::vector<uint8_t> cbor)
    : web_bundle_id_(std::move(web_bundle_id)), cbor_(std::move(cbor)) {}

IntegrityBlockAttributes::~IntegrityBlockAttributes() = default;

IntegrityBlockAttributes::IntegrityBlockAttributes(
    mojo::DefaultConstruct::Tag) {}

IntegrityBlockAttributes::IntegrityBlockAttributes(
    const IntegrityBlockAttributes&) = default;
IntegrityBlockAttributes& IntegrityBlockAttributes::operator=(
    const IntegrityBlockAttributes&) = default;

}  // namespace web_package
