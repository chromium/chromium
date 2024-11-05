// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/user_education_storage_service.h"

#include "base/feature_list.h"
#include "base/time/default_clock.h"
#include "components/user_education/common/user_education_data.h"

namespace user_education {

UserEducationTimeProvider::UserEducationTimeProvider()
    : clock_(base::DefaultClock::GetInstance()) {}

UserEducationTimeProvider::~UserEducationTimeProvider() = default;

base::Time UserEducationTimeProvider::GetCurrentTime() const {
  return clock_->Now();
}

UserEducationStorageService::UserEducationStorageService() = default;
UserEducationStorageService::~UserEducationStorageService() = default;

int UserEducationStorageService::GetSnoozeCount(
    const base::Feature& iph_feature) const {
  const auto data = ReadPromoData(iph_feature);
  return data ? data->snooze_count : 0;
}

KeyedFeaturePromoDataMap UserEducationStorageService::GetKeyedPromoData(
    const base::Feature& iph_feature) const {
  const auto data = ReadPromoData(iph_feature);
  if (!data) {
    return {};
  }

  return data->shown_for_keys;
}

}  // namespace user_education
