// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_METRICS_ID_ALLOCATOR_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_METRICS_ID_ALLOCATOR_H_

#include <optional>

#include "google_apis/gaia/gaia_id.h"

class SigninPrefs;

namespace signin {

// Returns the assigned ID for the given account.
// If the account has already been assigned an ID, returns that ID.
// Otherwise, allocates a new ID.
// Returns std::nullopt if the ID exceeds the cap (99).
// When an account is fully removed from Chrome and then re-added, it will
// receive a new ID. This is the expected behavior.
// A used ID is never reused.
std::optional<int> GetOrAllocateAccountMetricsId(SigninPrefs& signin_prefs,
                                                 const GaiaId& gaia_id);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_METRICS_ID_ALLOCATOR_H_
