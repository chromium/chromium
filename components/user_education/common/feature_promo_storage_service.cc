// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_storage_service.h"

#include "base/feature_list.h"
#include "base/time/default_clock.h"
#include "components/user_education/common/feature_promo_data.h"

namespace user_education {

FeaturePromoStorageService::FeaturePromoStorageService()
    : clock_(base::DefaultClock::GetInstance()) {}

FeaturePromoStorageService::~FeaturePromoStorageService() = default;

int FeaturePromoStorageService::GetSnoozeCount(
    const base::Feature& iph_feature) const {
  const auto data = ReadPromoData(iph_feature);
  return data ? data->snooze_count : 0;
}

KeyedFeaturePromoDataMap FeaturePromoStorageService::GetKeyedPromoData(
    const base::Feature& iph_feature) const {
  const auto data = ReadPromoData(iph_feature);
  if (!data) {
    return {};
  }

  return data->shown_for_keys;
}

base::Time FeaturePromoStorageService::GetCurrentTime() const {
  return clock_->Now();
}

}  // namespace user_education
