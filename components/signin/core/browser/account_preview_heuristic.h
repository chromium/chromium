// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

// Computes the preview preference (preferred data types and device form factor)
// for a single account preview data. Returns std::nullopt if the feature flag
// `switches::kEnableAccountPreviewPreferredAccount` is disabled.
std::optional<AccountPreviewDataService::AccountPreviewPreference>
ComputeAccountPreviewPreference(const GaiaId& gaia_id,
                                const AccountPreviewData& data);

// Input context needed by the heuristic to evaluate an account.
struct AccountPreviewHeuristicContext {
  GaiaId gaia_id;
  bool is_managed = false;
  bool is_child = false;
  bool is_external_app_primary = false;
  raw_ptr<const AccountPreviewData> preview_data = nullptr;

  bool is_eligible_for_preferred_account() const {
    return !is_managed && !is_child && preview_data != nullptr;
  }

  bool has_other_devices() const {
    return preview_data != nullptr && !preview_data->devices.empty();
  }
};

// Selects the preferred account for sign-in promo among the given list of
// accounts in the profile/device. `accounts[0]` is considered the default
// account.
// Ties are broken in favor of the earlier account in the list.
// Returns std::nullopt if no valid candidate is found or the list is empty.
std::optional<AccountPreviewDataService::AccountPreviewPreference>
ComputePreferredAccountForPromo(
    base::span<const AccountPreviewHeuristicContext> accounts);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
