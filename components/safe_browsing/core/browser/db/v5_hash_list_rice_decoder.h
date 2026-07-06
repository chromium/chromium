// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_HASH_LIST_RICE_DECODER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_HASH_LIST_RICE_DECODER_H_

#include <string>
#include <vector>

#include "components/safe_browsing/core/browser/db/v5_rice.h"

namespace safe_browsing {

namespace V5 {
class HashList;
}

namespace v5_hash_list_rice_decoder {

// Decodes the additions in `hash_list` and stores them in `raw_additions`.
// `raw_additions` when passed in must be empty.
// Returns `V5DecodeResult::kSuccess` if decoding succeeded, or an error code
// otherwise. If `hash_list` does not contain additions, it's considered
// successful.
// Callers are expected to validate `hash_list` using `ValidateHashList`
// before calling this method.
V5DecodeResult DecodeAdditions(const V5::HashList& hash_list,
                               std::string& raw_additions);

// Decodes the removals in `hash_list` and stores them in `decoded_removals`.
// `decoded_removals` when passed in must be empty.
// Returns `V5DecodeResult::kSuccess` if decoding succeeded, or an error code
// otherwise. If `hash_list` does not contain removals, it's considered
// successful.
// Callers are expected to validate `hash_list` using `ValidateHashList`
// before calling this method.
V5DecodeResult DecodeRemovals(const V5::HashList& hash_list,
                              std::vector<uint32_t>& decoded_removals);

// Validates the Rice-encoded parameters in `hash_list` without decoding.
// Returns `V5InputValidationResult::kSuccess` if the parameters are valid and
// a specific error otherwise.
V5InputValidationResult ValidateHashList(const V5::HashList& hash_list);

}  // namespace v5_hash_list_rice_decoder

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_HASH_LIST_RICE_DECODER_H_
