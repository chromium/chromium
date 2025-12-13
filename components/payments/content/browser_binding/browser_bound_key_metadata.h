// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_METADATA_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_METADATA_H_

#include <stdint.h>

#include <compare>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace payments {

// Describes the metadata stored about browser bound keys.
struct BrowserBoundKeyMetadata {
  // Describes the primary key of the browser bound key metadata. The browser
  // bound key is unique identified by the passkey's relying party id and its
  // credential_id.
  struct RelyingPartyAndCredentialId {
    // The relying party associated to the passkey.
    std::string relying_party_id;
    // The id of the passkey.
    std::vector<uint8_t> credential_id;

    RelyingPartyAndCredentialId();

    // Please use std::move() on `relying_party_id` and `credential_id` if
    // possible to avoid copying the string and vector.
    RelyingPartyAndCredentialId(std::string relying_party_id,
                                std::vector<uint8_t> credential_id);

    RelyingPartyAndCredentialId(const RelyingPartyAndCredentialId&);
    RelyingPartyAndCredentialId& operator=(const RelyingPartyAndCredentialId&);

    RelyingPartyAndCredentialId(RelyingPartyAndCredentialId&&);
    RelyingPartyAndCredentialId& operator=(RelyingPartyAndCredentialId&&);

    ~RelyingPartyAndCredentialId();

    std::strong_ordering operator<=>(
        const RelyingPartyAndCredentialId& other) const;
  };

  RelyingPartyAndCredentialId passkey;
  // The platform-specific id of the browser bound key.
  std::vector<uint8_t> browser_bound_key_id;
  // The last time the browser bound key was used.
  base::Time last_used;

  BrowserBoundKeyMetadata();

  BrowserBoundKeyMetadata(const BrowserBoundKeyMetadata&);
  BrowserBoundKeyMetadata& operator=(const BrowserBoundKeyMetadata&);

  BrowserBoundKeyMetadata(BrowserBoundKeyMetadata&&);
  BrowserBoundKeyMetadata& operator=(BrowserBoundKeyMetadata&&);

  ~BrowserBoundKeyMetadata();
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_METADATA_H_
