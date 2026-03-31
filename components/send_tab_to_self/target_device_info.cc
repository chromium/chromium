// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_info.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/send_tab_to_self/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

TargetDeviceInfo::TargetDeviceInfo(
    const std::string& device_name,
    const std::string& cache_guid,
    const syncer::DeviceInfo::FormFactor form_factor,
    base::Time last_updated_timestamp,
    bool has_high_precision_timestamp)
    : device_name(device_name),
      cache_guid(cache_guid),
      form_factor(form_factor),
      last_updated_timestamp(last_updated_timestamp),
      has_high_precision_timestamp(has_high_precision_timestamp) {}

TargetDeviceInfo::TargetDeviceInfo(const TargetDeviceInfo& other) = default;
TargetDeviceInfo::~TargetDeviceInfo() = default;

bool TargetDeviceInfo::operator==(const TargetDeviceInfo& rhs) const {
  return this->device_name == rhs.device_name &&
         this->cache_guid == rhs.cache_guid &&
         this->form_factor == rhs.form_factor &&
         this->last_updated_timestamp == rhs.last_updated_timestamp &&
         this->has_high_precision_timestamp == rhs.has_high_precision_timestamp;
}

std::u16string TargetDeviceInfo::GetLastActiveTimeForDisplay() const {
  base::TimeDelta delta = base::Time::Now() - last_updated_timestamp;
  if (delta.is_negative()) {
    delta = base::TimeDelta();
  }

  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfImprovedLastActiveLabels) &&
      has_high_precision_timestamp) {
    if (delta < base::Minutes(1)) {
      return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW);
    }

    if (delta < base::Hours(1)) {
      return l10n_util::GetPluralStringFUTF16(
          IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, delta.InMinutes());
    }

    if (delta < base::Days(1)) {
      return l10n_util::GetPluralStringFUTF16(
          IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, delta.InHours());
    }
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, delta.InDays());
}

}  // namespace send_tab_to_self
