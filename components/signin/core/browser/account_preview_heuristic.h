// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_

#include <optional>

#include "base/containers/flat_map.h"
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

// Reasons why an account was selected as the preferred account for sign-in
// promo.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AccountPreviewSelectionReason)
enum class AccountPreviewSelectionReason {
  // No account was selected (e.g. empty accounts list or feature disabled).
  kNoSelection = 0,
  // Priority 1: Default account is not a regular account (managed or child).
  kNonRegularDefault = 1,
  // Priority 2: An external app primary (AGA) regular account was selected.
  kExternalAppPrimary = 2,
  // Priority 3: Regular accounts were compared based on sync data score.
  kSyncDataScore = 3,

  kMaxValue = kSyncDataScore,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountPreviewSelectionReason)

// Result of evaluating the heuristic to select the preferred account for
// sign-in promo.
struct AccountPreviewSelectionResult {
  // The computed preference for the selected account (if an account was
  // selected and the feature is enabled).
  std::optional<AccountPreviewDataService::AccountPreviewPreference> preference;

  // The GaiaId of the selected account, or std::nullopt if none.
  std::optional<GaiaId> selected_account;

  // The reason why this account was selected.
  AccountPreviewSelectionReason selection_reason =
      AccountPreviewSelectionReason::kNoSelection;

  // Calculated sync data scores for evaluated accounts when `selection_reason`
  // is `kSyncDataScore`, mapped by GaiaId. Empty if score computation was not
  // performed (e.g. Priority 1 or 2 matched).
  base::flat_map<GaiaId, int> account_scores;
};

// Evaluates the heuristic across all candidate accounts in the profile to
// select the preferred account for sign-in promo.
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
// Returns a result with `preference = std::nullopt` and
// `selected_account = std::nullopt` if no valid candidate is found or
// `accounts` is empty.
AccountPreviewSelectionResult ComputePreferredAccountForPromo(
    base::span<const AccountPreviewHeuristicContext> accounts);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
