// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
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
  raw_ref<const AccountPreviewData> preview_data;
  bool is_managed = false;
  bool is_child = false;
  bool is_external_app_primary = false;

  // Returns whether this is a regular consumer account (i.e. not managed and
  // not a child account).
  bool is_regular_account() const { return !is_managed && !is_child; }

  bool has_other_devices() const { return !preview_data->devices.empty(); }
};

// Selects the preferred account for sign-in promo among the given list of
// accounts in the profile.
// The first account in the list (`accounts[0]`) must be the default account
// (i.e. the candidate account that would be promoted by default in the absence
// of account previews, as determined by
// `signin::GetOrderedAccountsForDisplay()`).
//
// Priorities:
// 1. If the default account is not a regular account, it is selected.
// 2. If an AGA (external app primary) account exists, it is selected.
// 3. Otherwise, compare sync data between all regular accounts. Select the best
//    account. Ties are broken in favor of the earlier account in the list
//    (favoring the default account).
//
// Returns std::nullopt if no valid candidate is found or the list is empty.
std::optional<AccountPreviewDataService::AccountPreviewPreference>
ComputePreferredAccountForPromo(
    base::span<const AccountPreviewHeuristicContext> accounts);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
