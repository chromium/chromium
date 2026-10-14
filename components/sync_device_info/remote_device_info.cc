// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/remote_device_info.h"

#include <algorithm>
#include <utility>

#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"

// Must come after all includes for JniType conversions.
#include "components/sync_device_info/jni_headers/RemoteDeviceInfo_jni.h"
#endif

namespace syncer {

RemoteDeviceInfo::RemoteDeviceInfo() = default;

RemoteDeviceInfo::RemoteDeviceInfo(std::string device_name,
                                   std::string cache_guid,
                                   DeviceInfo::FormFactor form_factor,
                                   DeviceInfo::OsType os_type,
                                   base::Time last_updated_timestamp,
                                   bool has_high_precision_timestamp)
    : device_name(std::move(device_name)),
      cache_guid(std::move(cache_guid)),
      form_factor(form_factor),
      os_type(os_type),
      last_updated_timestamp(last_updated_timestamp),
      has_high_precision_timestamp(has_high_precision_timestamp) {}

RemoteDeviceInfo::RemoteDeviceInfo(const RemoteDeviceInfo& other) = default;
RemoteDeviceInfo::~RemoteDeviceInfo() = default;

bool RemoteDeviceInfo::operator==(const RemoteDeviceInfo& rhs) const = default;

// static
std::u16string RemoteDeviceInfo::GetLastActiveTimeForDisplay(
    base::Time last_updated_timestamp,
    bool has_high_precision_timestamp) {
  const base::TimeDelta delta =
      std::max(base::TimeDelta(), base::Time::Now() - last_updated_timestamp);

  if (has_high_precision_timestamp) {
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

std::u16string RemoteDeviceInfo::GetLastActiveTimeForDisplay() const {
  return GetLastActiveTimeForDisplay(last_updated_timestamp,
                                     has_high_precision_timestamp);
}

}  // namespace syncer

#if BUILDFLAG(IS_ANDROID)
namespace jni_zero {

template <>
ScopedJavaLocalRef<jobject> ToJniType<syncer::RemoteDeviceInfo>(
    JNIEnv* env,
    const syncer::RemoteDeviceInfo& input) {
  return syncer::Java_RemoteDeviceInfo_Constructor(
      env, input.device_name, input.cache_guid, input.form_factor,
      input.os_type, input.GetLastActiveTimeForDisplay());
}

}  // namespace jni_zero

DEFINE_JNI(RemoteDeviceInfo)
#endif
