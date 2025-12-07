// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_metadata.h"

#include <compare>

namespace payments {

BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::
    RelyingPartyAndCredentialId() = default;

BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::
    RelyingPartyAndCredentialId(const RelyingPartyAndCredentialId&) = default;
BrowserBoundKeyMetadata::RelyingPartyAndCredentialId&
BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::operator=(
    const RelyingPartyAndCredentialId&) = default;

BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::
    RelyingPartyAndCredentialId(RelyingPartyAndCredentialId&&) = default;
BrowserBoundKeyMetadata::RelyingPartyAndCredentialId&
BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::operator=(
    RelyingPartyAndCredentialId&&) = default;

BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::
    RelyingPartyAndCredentialId(std::string relying_party_id,
                                std::vector<uint8_t> credential_id)
    : relying_party_id(std::move(relying_party_id)),
      credential_id(std::move(credential_id)) {}

BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::
    ~RelyingPartyAndCredentialId() = default;

std::strong_ordering
BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::operator<=>(
    const RelyingPartyAndCredentialId& other) const = default;

BrowserBoundKeyMetadata::BrowserBoundKeyMetadata() = default;

BrowserBoundKeyMetadata::BrowserBoundKeyMetadata(
    const BrowserBoundKeyMetadata&) = default;
BrowserBoundKeyMetadata& BrowserBoundKeyMetadata::operator=(
    const BrowserBoundKeyMetadata&) = default;

BrowserBoundKeyMetadata::BrowserBoundKeyMetadata(BrowserBoundKeyMetadata&&) =
    default;
BrowserBoundKeyMetadata& BrowserBoundKeyMetadata::operator=(
    BrowserBoundKeyMetadata&&) = default;

BrowserBoundKeyMetadata::~BrowserBoundKeyMetadata() = default;

}  // namespace payments
