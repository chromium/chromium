// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_storage_service.h"

#include "base/feature_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_education {

FeaturePromoStorageService::FeaturePromoStorageService() = default;
FeaturePromoStorageService::~FeaturePromoStorageService() = default;

int FeaturePromoStorageService::GetSnoozeCount(
    const base::Feature& iph_feature) const {
  const auto data = ReadPromoData(iph_feature);
  return data ? data->snooze_count : 0;
}

std::set<std::string> FeaturePromoStorageService::GetShownForApps(
    const base::Feature& iph_feature) const {
  const auto data = ReadPromoData(iph_feature);
  if (!data) {
    return std::set<std::string>();
  }

  return data->shown_for_apps;
}

FeaturePromoStorageService::PromoData::PromoData() = default;
FeaturePromoStorageService::PromoData::~PromoData() = default;
FeaturePromoStorageService::PromoData::PromoData(const PromoData&) = default;
FeaturePromoStorageService::PromoData::PromoData(PromoData&&) = default;
FeaturePromoStorageService::PromoData&
FeaturePromoStorageService::PromoData::operator=(const PromoData&) = default;
FeaturePromoStorageService::PromoData&
FeaturePromoStorageService::PromoData::operator=(PromoData&&) = default;

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoStorageService::CloseReason close_reason) {
  switch (close_reason) {
    case FeaturePromoStorageService::CloseReason::kDismiss:
      oss << "kDismiss";
      break;
    case FeaturePromoStorageService::CloseReason::kSnooze:
      oss << "kSnooze";
      break;
    case FeaturePromoStorageService::CloseReason::kAction:
      oss << "kAction";
      break;
    case FeaturePromoStorageService::CloseReason::kCancel:
      oss << "kCancel";
      break;
    case FeaturePromoStorageService::CloseReason::kTimeout:
      oss << "kTimeout";
      break;
    case FeaturePromoStorageService::CloseReason::kAbortPromo:
      oss << "kAbortPromo";
      break;
    case FeaturePromoStorageService::CloseReason::kFeatureEngaged:
      oss << "kFeatureEngaged";
      break;
    case FeaturePromoStorageService::CloseReason::kOverrideForUIRegionConflict:
      oss << "kOverrideForUIRegionConflict";
      break;
    case FeaturePromoStorageService::CloseReason::kOverrideForDemo:
      oss << "kOverrideForDemo";
      break;
    case FeaturePromoStorageService::CloseReason::kOverrideForTesting:
      oss << "kOverrideForTesting";
      break;
    case FeaturePromoStorageService::CloseReason::kOverrideForPrecedence:
      oss << "kOverrideForPrecedence";
      break;
  }
  return oss;
}

}  // namespace user_education
