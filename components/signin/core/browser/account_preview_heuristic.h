// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

// Computes the preview preference (preferred data types and device form factor)
// for a single account preview data.
std::optional<AccountPreviewDataService::AccountPreviewPreference>
ComputeAccountPreviewPreference(const GaiaId& gaia_id,
                                const AccountPreviewData& data);

// Input context needed by the heuristic to evaluate an account.
struct AccountPreviewHeuristicContext {
  GaiaId gaia_id;
  raw_ptr<const AccountPreviewData> preview_data = nullptr;
  bool is_primary = false;
  bool is_managed = false;
  bool is_child = false;
  bool is_external_app_primary = false;

  // Returns whether this is a regular consumer account (i.e. not managed and
  // not a child account).
  bool is_regular_account() const { return !is_managed && !is_child; }

  bool has_other_devices() const {
    return preview_data && !preview_data->devices.empty();
  }
};

// Reasons why an account was selected as the preferred account for sign-in
// promo.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AccountPreviewSelectionReason)
enum class AccountPreviewSelectionReason {
  // No account was selected (e.g. empty accounts list or missing default
  // account preview data).
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
  // The computed preference for the selected account, or std::nullopt if no
  // account was selected.
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

// Outcomes of evaluating the heuristic to select a secondary account for
// account switching promo.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AccountSwitchingSelectionOutcome)
enum class AccountSwitchingSelectionOutcome {
  // Promo would be shown: Primary account has a low base score (0 to 2), and
  // the best secondary account exceeds `primary_score`.
  kWouldShowLowPrimaryScore = 0,
  // Promo would be shown: Primary account has a medium base score (3 to 5), and
  // the best secondary account exceeds `2 * primary_score`.
  kWouldShowDoubledPrimaryScore = 1,
  // Promo would not be shown: Primary account has a base score < 6, but the
  // best secondary account does not meet the required score threshold.
  kWouldNotShowSecondaryDoesNotMeetThreshold = 2,
  // Promo would not be shown: Primary account base score is >= 6 (cutoff).
  kWouldNotShowPrimaryExceedsUpperLimit = 3,
  // Promo would not be shown: Primary account is not a regular account
  // (managed or child).
  kWouldNotShowNonRegularPrimaryAccount = 4,
  // Promo would not be shown: No regular secondary account is available to
  // switch to (all secondary accounts are managed or child).
  kWouldNotShowNoRegularSecondaryAccount = 5,
  // Promo would not be shown: Primary account is an external app primary (AGA)
  // account.
  kWouldNotShowExternalAppPrimaryAccount = 6,
  // Promo would not be shown: Preconditions not met (fewer than 2 accounts).
  kWouldNotShowNotEnoughAccounts = 7,
  // Promo would not be shown: No primary account exists.
  kWouldNotShowNoPrimaryAccount = 8,
  // Promo would not be shown: No regular secondary account is signed in to
  // another device (`has_other_devices()` is false).
  kWouldNotShowNoSecondaryWithOtherDevices = 9,
  // Promo would not be shown: Preview data is missing (e.g. fetch failed) for
  // the primary account.
  kWouldNotShowPrimaryMissingPreviewData = 10,
  // Promo would not be shown: Preview data is missing (e.g. fetch failed) for
  // all regular secondary accounts.
  kWouldNotShowSecondaryMissingPreviewData = 11,

  kMaxValue = kWouldNotShowSecondaryMissingPreviewData,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountSwitchingSelectionOutcome)

// Result of evaluating the heuristic to select a secondary account for
// switching promo.
struct AccountSwitchingSelectionResult {
  // The computed preference for the selected switching account (if an account
  // was selected).
  std::optional<AccountPreviewDataService::AccountPreviewPreference> preference;

  // The GaiaId of the selected switching account, or std::nullopt if none.
  std::optional<GaiaId> selected_account;

  // The outcome of the switching selection evaluation.
  AccountSwitchingSelectionOutcome outcome =
      AccountSwitchingSelectionOutcome::kWouldNotShowNotEnoughAccounts;
};

// Evaluates the heuristic across all candidate accounts in the profile to
// select a secondary account to promote for account switching.
// `accounts` should be ordered by display priority (as returned by
// `signin::GetOrderedAccountsForDisplay()`), where `accounts[0]` is the
// primary account if one exists.
//
// Evaluation order (the first matching condition determines `outcome`):
// 1. A primary account must exist (`accounts[0].is_primary`) -> otherwise
//    `kWouldNotShowNoPrimaryAccount`.
// 2. At least 2 accounts must exist -> otherwise
//    `kWouldNotShowNotEnoughAccounts`.
// 3. Primary account must be a regular account -> otherwise
//    `kWouldNotShowNonRegularPrimaryAccount`.
// 4. Primary account must not be an external app primary (AGA) account ->
//    otherwise `kWouldNotShowExternalAppPrimaryAccount`.
// 5. Primary account must have preview data -> otherwise
//    `kWouldNotShowPrimaryMissingPreviewData`.
// 6. At least one regular secondary account must exist -> otherwise
//    `kWouldNotShowNoRegularSecondaryAccount`.
// 7. At least one regular secondary account must have preview data -> otherwise
//    `kWouldNotShowSecondaryMissingPreviewData`.
// 8. At least one regular secondary account must be signed in to another
//    device -> otherwise `kWouldNotShowNoSecondaryWithOtherDevices`.
// 9. Primary account score must be below 6 -> otherwise
//    `kWouldNotShowPrimaryExceedsUpperLimit`.
// 10. Compare the highest-scoring cross-device regular secondary account (X)
//    against the primary account score (Y):
//    - For Y in {0, 2}: select secondary if X > Y
//    (`kWouldShowLowPrimaryScore`).
//    - For Y in {3, 5}: select secondary if X > 2 * Y
//      (`kWouldShowDoubledPrimaryScore`).
//    - Otherwise, `kWouldNotShowSecondaryDoesNotMeetThreshold`.
AccountSwitchingSelectionResult ComputeAccountSwitchingSelection(
    base::span<const AccountPreviewHeuristicContext> accounts);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_HEURISTIC_H_
