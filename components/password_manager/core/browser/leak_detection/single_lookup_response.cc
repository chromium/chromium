// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"

namespace password_manager {

SingleLookupResponse::SingleLookupResponse() = default;

SingleLookupResponse::SingleLookupResponse(const SingleLookupResponse& other) =
    default;

SingleLookupResponse& SingleLookupResponse::operator=(
    const SingleLookupResponse& other) = default;

SingleLookupResponse::SingleLookupResponse(SingleLookupResponse&& other) =
    default;

SingleLookupResponse& SingleLookupResponse::operator=(
    SingleLookupResponse&& other) = default;

SingleLookupResponse::~SingleLookupResponse() = default;

bool operator==(const SingleLookupResponse& lhs,
                const SingleLookupResponse& rhs) {
  return lhs.encrypted_leak_match_prefixes ==
             rhs.encrypted_leak_match_prefixes &&
         lhs.reencrypted_lookup_hash == rhs.reencrypted_lookup_hash;
}

}  // namespace password_manager
