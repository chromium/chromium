// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_heuristic.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"

namespace signin {

namespace {

// Maximum primary account sync data score at or above which account switching
// is never offered.
constexpr int kSwitchingPrimaryScoreUpperLimit = 6;

// Upper bound for the low primary account sync data score bracket (0 to 2),
// where the secondary account score must exceed `primary_score`.
constexpr int kSwitchingLowPrimaryScoreThreshold = 2;

// Threshold values for quartile classification of sync data counts.
struct SyncDataTypeThresholds {
  size_t q1 = 0;
  size_t median = 0;
  size_t q3 = 0;

  SyncDataQuartile GetQuartileForCount(size_t count) const {
    if (count == 0) {
      return SyncDataQuartile::kZero;
    }
    if (count >= q3) {
      return SyncDataQuartile::kAboveQ3;
    }
    if (count >= median) {
      return SyncDataQuartile::kMedianToQ3;
    }
    if (count >= q1) {
      return SyncDataQuartile::kQ1ToMedian;
    }
    return SyncDataQuartile::kBelowQ1;
  }
};

std::pair<syncer::DataType, SyncDataTypeThresholds> GetDataTypeThresholdEntry(
    syncer::DataType type) {
  switch (type) {
    case syncer::PASSWORDS:
      return {syncer::PASSWORDS,
              {.q1 = switches::kPasswordsQ1Threshold.Get(),
               .median = switches::kPasswordsMedianThreshold.Get(),
               .q3 = switches::kPasswordsQ3Threshold.Get()}};
    case syncer::BOOKMARKS:
      return {syncer::BOOKMARKS,
              {.q1 = switches::kBookmarksQ1Threshold.Get(),
               .median = switches::kBookmarksMedianThreshold.Get(),
               .q3 = switches::kBookmarksQ3Threshold.Get()}};
    case syncer::AUTOFILL:
      return {syncer::AUTOFILL,
              {.q1 = switches::kAutofillQ1Threshold.Get(),
               .median = switches::kAutofillMedianThreshold.Get(),
               .q3 = switches::kAutofillQ3Threshold.Get()}};
    case syncer::AUTOFILL_WALLET_METADATA:
      return {syncer::AUTOFILL_WALLET_METADATA,
              {.q1 = switches::kAutofillWalletMetadataQ1Threshold.Get(),
               .median = switches::kAutofillWalletMetadataMedianThreshold.Get(),
               .q3 = switches::kAutofillWalletMetadataQ3Threshold.Get()}};
    case syncer::READING_LIST:
      return {syncer::READING_LIST,
              {.q1 = switches::kReadingListQ1Threshold.Get(),
               .median = switches::kReadingListMedianThreshold.Get(),
               .q3 = switches::kReadingListQ3Threshold.Get()}};
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case syncer::EXTENSIONS:
      return {syncer::EXTENSIONS,
              {.q1 = switches::kExtensionsQ1Threshold.Get(),
               .median = switches::kExtensionsMedianThreshold.Get(),
               .q3 = switches::kExtensionsQ3Threshold.Get()}};
#endif
    default:
      NOTREACHED();
  }
}

// Returns the list of sync data types and their respective thresholds used for
// quartile classification when computing sync data score.
const auto& GetScoreDataTypeThresholds() {
  static const auto kScoreDataTypeThresholds =
      std::to_array<std::pair<syncer::DataType, SyncDataTypeThresholds>>({
          GetDataTypeThresholdEntry(syncer::PASSWORDS),
          GetDataTypeThresholdEntry(syncer::BOOKMARKS),
          GetDataTypeThresholdEntry(syncer::AUTOFILL),
          GetDataTypeThresholdEntry(syncer::AUTOFILL_WALLET_METADATA),
      });
  return kScoreDataTypeThresholds;
}

// Returns the list of all relevant sync data types and their respective
// thresholds for quartile classification when determining preferred data
// types for string personalization.
const auto& GetPreferredDataTypeThresholds() {
  static const auto kPreferredDataTypeThresholds =
      std::to_array<std::pair<syncer::DataType, SyncDataTypeThresholds>>({
          GetDataTypeThresholdEntry(syncer::PASSWORDS),
          GetDataTypeThresholdEntry(syncer::BOOKMARKS),
          GetDataTypeThresholdEntry(syncer::AUTOFILL),
          GetDataTypeThresholdEntry(syncer::AUTOFILL_WALLET_METADATA),
          GetDataTypeThresholdEntry(syncer::READING_LIST),
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
          GetDataTypeThresholdEntry(syncer::EXTENSIONS),
#endif
      });
  return kPreferredDataTypeThresholds;
}

std::vector<PreferredDataTypeInfo> ExtractPreferredDataTypes(
    const AccountPreviewData& data) {
  struct DataTypeCandidate {
    syncer::DataType type = syncer::DataType::UNSPECIFIED;
    SyncDataQuartile quartile = SyncDataQuartile::kZero;
    double median_ratio = 0.;
  };

  // Extract data types with counts and calculate median ratios.
  std::vector<DataTypeCandidate> candidates;
  for (const auto& [type, thresholds] : GetPreferredDataTypeThresholds()) {
    auto it = data.counts.find(type);
    if (it != data.counts.end() && it->second > 0) {
      size_t count = it->second;
      CHECK_GT(thresholds.median, 0u);
      double ratio = static_cast<double>(count) / thresholds.median;
      candidates.push_back({
          .type = type,
          .quartile = thresholds.GetQuartileForCount(count),
          .median_ratio = ratio,
      });
    }
  }

  // Sort by the most relevant data types, relatives to their median count.
  // In case tie breakers, keeping the order of the original array for priority.
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const DataTypeCandidate& a, const DataTypeCandidate& b) {
                     return a.median_ratio > b.median_ratio;
                   });

  std::vector<PreferredDataTypeInfo> result;
  result.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    result.push_back({
        .data_type = candidate.type,
        .quartile = candidate.quartile,
    });
  }
  return result;
}

PreferredDeviceInfo ExtractPreferredOtherDevice(
    const AccountPreviewData& data) {
  const DevicePreview* most_recent_device = nullptr;
  for (const auto& device : data.devices) {
    if (device.form_factor ==
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED) {
      continue;
    }
    if (!most_recent_device ||
        device.last_updated > most_recent_device->last_updated) {
      most_recent_device = &device;
    }
  }

  if (!most_recent_device) {
    return {};
  }

  return PreferredDeviceInfo{
      .form_factor = most_recent_device->form_factor,
      .enabled_data_types = most_recent_device->interested_data_types,
  };
}

// This function is needed as `SyncDataQuartileToValue()` should only be used
// for persisting information, and not comparing score, since the value of the
// enum may not represent its semantic meaning of magnitude.
// The score uses powers of 2 (exponential) to give higher importance to higher
// quartiles.
int GetQuartileScore(SyncDataQuartile quartile) {
  switch (quartile) {
    case SyncDataQuartile::kZero:
      return 0;
    case SyncDataQuartile::kBelowQ1:
      return 1;
    case SyncDataQuartile::kQ1ToMedian:
      return 2;
    case SyncDataQuartile::kMedianToQ3:
      return 4;
    case SyncDataQuartile::kAboveQ3:
      return 8;
  }
  NOTREACHED();
}

// Holds the total sync data score and individual quartile counts for an
// account. In case of equal scores (ties), the count of higher quartiles is
// used as a tie-breaker.
struct SyncDataScore {
  int total_score = 0;
  size_t q4_count = 0;
  size_t q3_count = 0;
  size_t q2_count = 0;
  size_t q1_count = 0;

  auto operator<=>(const SyncDataScore&) const = default;
};

// The Account Score is computed as the sum of each data type quartile score.
// The Quartile score is an exponential mapping of the quartile to an integer
// value (powers of 2), check `GetQuartileScore()`.
// In case of ties, the counts of higher quartiles break the tie.
SyncDataScore CalculateSyncDataScore(const AccountPreviewData& data) {
  SyncDataScore score;
  for (const auto& [type, thresholds] : GetScoreDataTypeThresholds()) {
    auto it = data.counts.find(type);
    if (it != data.counts.end()) {
      SyncDataQuartile quartile = thresholds.GetQuartileForCount(it->second);
      score.total_score += GetQuartileScore(quartile);
      switch (quartile) {
        case SyncDataQuartile::kZero:
          break;
        case SyncDataQuartile::kBelowQ1:
          score.q1_count++;
          break;
        case SyncDataQuartile::kQ1ToMedian:
          score.q2_count++;
          break;
        case SyncDataQuartile::kMedianToQ3:
          score.q3_count++;
          break;
        case SyncDataQuartile::kAboveQ3:
          score.q4_count++;
          break;
      }
    }
  }
  return score;
}

bool HasEqualOrMoreSyncData(const AccountPreviewData& candidate,
                            const AccountPreviewData& base) {
  return CalculateSyncDataScore(candidate) >= CalculateSyncDataScore(base);
}

bool HasStrictlyMoreSyncData(const AccountPreviewData& candidate,
                             const AccountPreviewData& base) {
  return CalculateSyncDataScore(candidate) > CalculateSyncDataScore(base);
}

bool IsCandidatePreferredOverCurrentBest(
    const AccountPreviewHeuristicContext& candidate,
    const AccountPreviewHeuristicContext& current_best) {
  CHECK(candidate.is_regular_account());
  CHECK(current_best.is_regular_account());

  bool candidate_cross_device = candidate.has_other_devices();
  bool best_cross_device = current_best.has_other_devices();

  // Candidate is cross-device, Current Best is single-device.
  if (candidate_cross_device && !best_cross_device) {
    return HasEqualOrMoreSyncData(*candidate.preview_data,
                                  *current_best.preview_data);
  }

  // Candidate is cross-device, Current Best is cross-device.
  if (candidate_cross_device && best_cross_device) {
    return HasStrictlyMoreSyncData(*candidate.preview_data,
                                   *current_best.preview_data);
  }

  // Candidate is single-device, Current Best is single-device.
  if (!candidate_cross_device && !best_cross_device) {
    return HasStrictlyMoreSyncData(*candidate.preview_data,
                                   *current_best.preview_data);
  }

  // Candidate is single-device, Current Best is cross-device.
  return false;
}

}  // namespace

std::optional<AccountPreviewDataService::AccountPreviewPreference>
ComputeAccountPreviewPreference(const GaiaId& gaia_id,
                                const AccountPreviewData& data) {
  AccountPreviewDataService::AccountPreviewPreference preference;
  preference.gaia_id = gaia_id;
  preference.preferred_data_types = ExtractPreferredDataTypes(data);
  preference.other_device_info = ExtractPreferredOtherDevice(data);
  return preference;
}

AccountPreviewSelectionResult ComputePreferredAccountForPromo(
    base::span<const AccountPreviewHeuristicContext> accounts) {

  if (accounts.empty() || !accounts[0].preview_data) {
    return {};
  }

  AccountPreviewSelectionResult result;

  // The first account in the list (`accounts[0]`) is the default account (the
  // account that would be promoted by default in the absence of previews).
  // Priority 1: If the default account is not a regular account, select it.
  const AccountPreviewHeuristicContext& default_account = accounts[0];
  if (!default_account.is_regular_account()) {
    result.selected_account = default_account.gaia_id;
    result.selection_reason = AccountPreviewSelectionReason::kNonRegularDefault;
    result.preference = ComputeAccountPreviewPreference(
        default_account.gaia_id, *default_account.preview_data);
    return result;
  }

  // Priority 2: If an AGA (external app primary) account exists, select it.
  for (const auto& account : accounts) {
    if (account.preview_data && account.is_external_app_primary &&
        account.is_regular_account()) {
      result.selected_account = account.gaia_id;
      result.selection_reason =
          AccountPreviewSelectionReason::kExternalAppPrimary;
      result.preference = ComputeAccountPreviewPreference(
          account.gaia_id, *account.preview_data);
      return result;
    }
  }

  // Priority 3: Compare sync data between all regular accounts.
  const AccountPreviewHeuristicContext* best_candidate = nullptr;
  for (const auto& account : accounts) {
    if (!account.preview_data || !account.is_regular_account()) {
      continue;
    }
    result.account_scores[account.gaia_id] =
        CalculateSyncDataScore(*account.preview_data).total_score;
    if (!best_candidate ||
        IsCandidatePreferredOverCurrentBest(account, *best_candidate)) {
      best_candidate = &account;
    }
  }

  // At this point, the default account (`accounts[0]`) is guaranteed to be a
  // regular account (otherwise Priority 1 would have matched), so
  // `best_candidate` is guaranteed to be non-null.
  CHECK(best_candidate);
  result.selected_account = best_candidate->gaia_id;
  result.selection_reason = AccountPreviewSelectionReason::kSyncDataScore;
  result.preference = ComputeAccountPreviewPreference(
      best_candidate->gaia_id, *best_candidate->preview_data);
  return result;
}

AccountSwitchingSelectionResult ComputeAccountSwitchingSelection(
    base::span<const AccountPreviewHeuristicContext> accounts) {
  AccountSwitchingSelectionResult result;
  if (accounts.empty() || !accounts[0].is_primary) {
    result.outcome =
        AccountSwitchingSelectionOutcome::kWouldNotShowNoPrimaryAccount;
    return result;
  }

  if (accounts.size() < 2) {
    result.outcome =
        AccountSwitchingSelectionOutcome::kWouldNotShowNotEnoughAccounts;
    return result;
  }

  // The first account in the list (`accounts[0]`) is the primary account.
  const AccountPreviewHeuristicContext& primary_account = accounts[0];
  if (!primary_account.is_regular_account()) {
    result.outcome =
        AccountSwitchingSelectionOutcome::kWouldNotShowNonRegularPrimaryAccount;
    return result;
  }

  if (primary_account.is_external_app_primary) {
    result.outcome = AccountSwitchingSelectionOutcome::
        kWouldNotShowExternalAppPrimaryAccount;
    return result;
  }

  if (!primary_account.preview_data) {
    result.outcome = AccountSwitchingSelectionOutcome::
        kWouldNotShowPrimaryMissingPreviewData;
    return result;
  }

  bool has_regular_secondary = false;
  bool has_regular_secondary_with_preview_data = false;
  const AccountPreviewHeuristicContext* best_secondary = nullptr;
  SyncDataScore best_secondary_score;
  for (const auto& account : accounts.subspan(1u)) {
    if (!account.is_regular_account()) {
      continue;
    }
    has_regular_secondary = true;
    if (!account.preview_data) {
      continue;
    }
    has_regular_secondary_with_preview_data = true;
    if (!account.has_other_devices()) {
      continue;
    }
    SyncDataScore score = CalculateSyncDataScore(*account.preview_data);
    if (!best_secondary || score > best_secondary_score) {
      best_secondary = &account;
      best_secondary_score = score;
    }
  }

  if (!has_regular_secondary) {
    result.outcome = AccountSwitchingSelectionOutcome::
        kWouldNotShowNoRegularSecondaryAccount;
    return result;
  }

  if (!has_regular_secondary_with_preview_data) {
    result.outcome = AccountSwitchingSelectionOutcome::
        kWouldNotShowSecondaryMissingPreviewData;
    return result;
  }

  if (!best_secondary) {
    result.outcome = AccountSwitchingSelectionOutcome::
        kWouldNotShowNoSecondaryWithOtherDevices;
    return result;
  }

  const int primary_score =
      CalculateSyncDataScore(*primary_account.preview_data).total_score;
  const int secondary_score = best_secondary_score.total_score;

  if (primary_score >= kSwitchingPrimaryScoreUpperLimit) {
    result.outcome =
        AccountSwitchingSelectionOutcome::kWouldNotShowPrimaryExceedsUpperLimit;
    return result;
  }

  // For primary score Y in {0, 2}, secondary score X must satisfy X > Y.
  // For primary score Y in {3, 5}, secondary score X must satisfy X > 2 * Y.
  if (primary_score <= kSwitchingLowPrimaryScoreThreshold) {
    if (secondary_score > primary_score) {
      result.outcome =
          AccountSwitchingSelectionOutcome::kWouldShowLowPrimaryScore;
      result.selected_account = best_secondary->gaia_id;
      result.preference = ComputeAccountPreviewPreference(
          best_secondary->gaia_id, *best_secondary->preview_data);
      return result;
    }
  } else {
    CHECK_LT(primary_score, kSwitchingPrimaryScoreUpperLimit);
    if (secondary_score > 2 * primary_score) {
      result.outcome =
          AccountSwitchingSelectionOutcome::kWouldShowDoubledPrimaryScore;
      result.selected_account = best_secondary->gaia_id;
      result.preference = ComputeAccountPreviewPreference(
          best_secondary->gaia_id, *best_secondary->preview_data);
      return result;
    }
  }

  result.outcome = AccountSwitchingSelectionOutcome::
      kWouldNotShowSecondaryDoesNotMeetThreshold;
  return result;
}

}  // namespace signin
