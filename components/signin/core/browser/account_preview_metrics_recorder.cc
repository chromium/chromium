// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_metrics_recorder.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/signin/core/browser/account_metrics_id_allocator.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"

namespace signin {

namespace {

enum class CountScale { k100, k1000, k10000, k100000 };

std::optional<CountScale> GetScaleForDataType(syncer::DataType type) {
  switch (type) {
    case syncer::APPS:
      return CountScale::k100;
    case syncer::AUTOFILL:
      return CountScale::k100000;
    case syncer::AUTOFILL_WALLET_CREDENTIAL:
      return CountScale::k100;
    case syncer::BOOKMARKS:
      return CountScale::k100000;
    case syncer::EXTENSIONS:
      return CountScale::k100;
    case syncer::PASSWORDS:
      return CountScale::k10000;
    case syncer::PREFERENCES:
      return CountScale::k1000;
    case syncer::READING_LIST:
      return CountScale::k1000;
    case syncer::SESSIONS:
      return CountScale::k10000;
    case syncer::THEMES:
      return CountScale::k100;
    case syncer::AUTOFILL_WALLET_METADATA:
      return CountScale::k100;
    case syncer::DEVICE_INFO:
      return CountScale::k1000;
    default:
      return std::nullopt;
  }
}

}  // namespace

AccountPreviewMetricsRecorder::AccountPreviewMetricsRecorder(
    PrefService& pref_service,
    const IdentityManager& identity_manager,
    const metrics::ProfileMetricsService& profile_metrics_service)
    : signin_prefs_(pref_service),
      identity_manager_(identity_manager),
      profile_metrics_service_(profile_metrics_service) {}

AccountPreviewMetricsRecorder::~AccountPreviewMetricsRecorder() = default;

void AccountPreviewMetricsRecorder::RecordMetrics(
    const GaiaId& gaia_id,
    const AccountPreviewData& data) {
  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id);
  if (account_info.IsEmpty()) {
    return;
  }

  std::optional<int> allocated_account_index =
      GetOrAllocateAccountMetricsId(signin_prefs_, gaia_id);
  if (!allocated_account_index || *allocated_account_index >= 5) {
    return;
  }
  int account_index = *allocated_account_index;
  std::string account_suffix = ".Account" + base::NumberToString(account_index);

  bool is_primary = identity_manager_->GetPrimaryAccountId(
                        ConsentLevel::kSignin) == account_info.account_id;
  // TODO(crbug.com/510760810): Add HasOtherDevices metric once the network
  // response parsing is implemented.

  if (account_info.IsManaged() != Tribool::kUnknown) {
    profile_metrics_service_->UmaHistogramBoolean(
        base::StrCat(
            {"Signin.SmartAccountSelection.OnSyncPreviewFetched.IsManaged",
             account_suffix}),
        account_info.IsManaged() == Tribool::kTrue);
  }
  if (account_info.IsChildAccount() != Tribool::kUnknown) {
    profile_metrics_service_->UmaHistogramBoolean(
        base::StrCat(
            {"Signin.SmartAccountSelection.OnSyncPreviewFetched.IsSupervised",
             account_suffix}),
        account_info.IsChildAccount() == Tribool::kTrue);
  }
  profile_metrics_service_->UmaHistogramBoolean(
      base::StrCat(
          {"Signin.SmartAccountSelection.OnSyncPreviewFetched.IsPrimary",
           account_suffix}),
      is_primary);

  for (const auto& [type, count] : data.counts) {
    std::optional<CountScale> scale = GetScaleForDataType(type);
    if (!scale.has_value()) {
      continue;
    }
    std::string base_name =
        base::StrCat({"Signin.SmartAccountSelection.OnSyncPreviewFetched.",
                      syncer::DataTypeToHistogramSuffix(type), account_suffix});
    int sample_count = base::saturated_cast<int>(count);
    switch (*scale) {
      case CountScale::k100:
        profile_metrics_service_->UmaHistogramCounts100(base_name,
                                                        sample_count);
        break;
      case CountScale::k1000:
        profile_metrics_service_->UmaHistogramCounts1000(base_name,
                                                         sample_count);
        break;
      case CountScale::k10000:
        profile_metrics_service_->UmaHistogramCounts10000(base_name,
                                                          sample_count);
        break;
      case CountScale::k100000:
        profile_metrics_service_->UmaHistogramCounts100000(base_name,
                                                           sample_count);
        break;
    }
  }
}

}  // namespace signin
