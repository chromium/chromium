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

  SyncDataQuartile GetQuartileScore(size_t count) const {
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

// List of all relevant sync data types and their respective default
// thresholds for quartile classification, in priority order for tie-breaking.
// TODO(crbug.com/530144650): Add real counts for these thresholds.
constexpr auto kDataTypeThresholds =
    std::to_array<std::pair<syncer::DataType, SyncDataTypeThresholds>>({
        {syncer::PASSWORDS, {.q1 = 5, .median = 20, .q3 = 50}},
        {syncer::BOOKMARKS, {.q1 = 10, .median = 50, .q3 = 150}},
        {syncer::AUTOFILL, {.q1 = 5, .median = 15, .q3 = 40}},
        {syncer::AUTOFILL_WALLET_METADATA, {.q1 = 1, .median = 3, .q3 = 8}},
    });

std::vector<PreferredDataTypeInfo> ExtractPreferredDataTypes(
    const AccountPreviewData& data) {
  struct DataTypeCandidate {
    syncer::DataType type = syncer::DataType::UNSPECIFIED;
    SyncDataQuartile quartile = SyncDataQuartile::kZero;
    double median_ratio = 0.;
  };

  // Extract data types with counts and calculate median ratios.
  std::vector<DataTypeCandidate> candidates;
  for (const auto& [type, thresholds] : kDataTypeThresholds) {
    auto it = data.counts.find(type);
    if (it != data.counts.end() && it->second > 0) {
      size_t count = it->second;
      CHECK_GT(thresholds.median, 0u);
      double ratio = static_cast<double>(count) / thresholds.median;
      candidates.push_back({
          .type = type,
          .quartile = thresholds.GetQuartileScore(count),
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

}  // namespace signin
