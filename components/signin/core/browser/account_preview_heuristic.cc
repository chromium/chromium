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
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"

namespace signin {

namespace {

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

// Returns the list of all relevant sync data types and their respective
// thresholds for quartile classification, in priority order for tie-breaking.
const auto& GetDataTypeThresholds() {
  static const auto kDataTypeThresholds =
      std::to_array<std::pair<syncer::DataType, SyncDataTypeThresholds>>({
          {syncer::PASSWORDS,
           SyncDataTypeThresholds{
               .q1 = switches::kPasswordsQ1Threshold.Get(),
               .median = switches::kPasswordsMedianThreshold.Get(),
               .q3 = switches::kPasswordsQ3Threshold.Get()}},
          {syncer::BOOKMARKS,
           SyncDataTypeThresholds{
               .q1 = switches::kBookmarksQ1Threshold.Get(),
               .median = switches::kBookmarksMedianThreshold.Get(),
               .q3 = switches::kBookmarksQ3Threshold.Get()}},
          {syncer::AUTOFILL,
           SyncDataTypeThresholds{
               .q1 = switches::kAutofillQ1Threshold.Get(),
               .median = switches::kAutofillMedianThreshold.Get(),
               .q3 = switches::kAutofillQ3Threshold.Get()}},
          {syncer::AUTOFILL_WALLET_METADATA,
           SyncDataTypeThresholds{
               .q1 = switches::kAutofillWalletMetadataQ1Threshold.Get(),
               .median = switches::kAutofillWalletMetadataMedianThreshold.Get(),
               .q3 = switches::kAutofillWalletMetadataQ3Threshold.Get()}},
      });
  return kDataTypeThresholds;
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
  for (const auto& [type, thresholds] : GetDataTypeThresholds()) {
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

sync_pb::SyncEnums_DeviceFormFactor ExtractOtherDeviceFormFactor(
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

  return most_recent_device
             ? most_recent_device->form_factor
             : sync_pb::
                   SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED;
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
  for (const auto& [type, thresholds] : GetDataTypeThresholds()) {
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

bool IsCandidatePreferredOverDefault(
    const AccountPreviewHeuristicContext& candidate,
    const AccountPreviewHeuristicContext& default_account) {
  if (!candidate.is_eligible_for_preferred_account()) {
    return false;
  }

  if (!default_account.is_eligible_for_preferred_account()) {
    return true;
  }

  // Comparison involving the external app (AGA) primary account on Android.
  // The AGA primary account is always preferred, UNLESS the competing account
  // is cross-device AND has strictly more sync data.
  if (candidate.is_external_app_primary) {
    if (default_account.has_other_devices() &&
        HasStrictlyMoreSyncData(*default_account.preview_data,
                                *candidate.preview_data)) {
      return false;
    }
    return true;
  }

  if (default_account.is_external_app_primary) {
    if (candidate.has_other_devices() &&
        HasStrictlyMoreSyncData(*candidate.preview_data,
                                *default_account.preview_data)) {
      return true;
    }
    return false;
  }

  bool candidate_cross_device = candidate.has_other_devices();
  bool default_cross_device = default_account.has_other_devices();

  // Candidate is cross-device, Default is single-device.
  if (candidate_cross_device && !default_cross_device) {
    return HasEqualOrMoreSyncData(*candidate.preview_data,
                                  *default_account.preview_data);
  }

  // Candidate is cross-device, Default is cross-device.
  if (candidate_cross_device && default_cross_device) {
    return HasStrictlyMoreSyncData(*candidate.preview_data,
                                   *default_account.preview_data);
  }

  // Candidate is single-device, Default is single-device.
  if (!candidate_cross_device && !default_cross_device) {
    return HasStrictlyMoreSyncData(*candidate.preview_data,
                                   *default_account.preview_data);
  }

  // Candidate is single-device, Default is cross-device.
  return false;
}

}  // namespace

std::optional<AccountPreviewDataService::AccountPreviewPreference>
ComputeAccountPreviewPreference(const GaiaId& gaia_id,
                                const AccountPreviewData& data) {
  if (!base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewPreferredAccount)) {
    return std::nullopt;
  }

  AccountPreviewDataService::AccountPreviewPreference preference;
  preference.gaia_id = gaia_id;
  preference.preferred_data_types = ExtractPreferredDataTypes(data);
  preference.other_device_form_factor = ExtractOtherDeviceFormFactor(data);
  return preference;
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
ComputePreferredAccountForPromo(
    base::span<const AccountPreviewHeuristicContext> accounts) {
  if (!base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewPreferredAccount)) {
    return std::nullopt;
  }

  if (accounts.empty()) {
    return std::nullopt;
  }

  size_t best_index = 0;
  for (size_t i = 1; i < accounts.size(); ++i) {
    if (IsCandidatePreferredOverDefault(accounts[i], accounts[best_index])) {
      best_index = i;
    }
  }

  const AccountPreviewHeuristicContext& chosen = accounts[best_index];
  // This may happen if all accounts in the list are ineligible.
  if (!chosen.is_eligible_for_preferred_account()) {
    return std::nullopt;
  }

  return ComputeAccountPreviewPreference(chosen.gaia_id, *chosen.preview_data);
}

}  // namespace signin
