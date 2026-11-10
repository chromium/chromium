// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/remote_device_info.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/device_name_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"

// Must come after all includes for JniType conversions.
#include "components/sync_device_info/jni_headers/RemoteDeviceInfo_jni.h"
#endif

namespace syncer {

namespace {

struct DeviceWithTimestamp {
  raw_ptr<const DeviceInfo> device;
  base::Time last_active;
  bool has_high_precision = false;
};

// Returns a list of devices with the last active timestamp for each device.
// The last active timestamp is the maximum of the device's last updated
// timestamp and the last modified time of any session on the device.
std::vector<DeviceWithTimestamp> GetDevicesWithLastActiveTime(
    const std::vector<const DeviceInfo*>& all_devices,
    const base::flat_map<std::string, base::Time>&
        session_last_modified_times) {
  return base::ToVector(
      all_devices, [&session_last_modified_times](const DeviceInfo* device) {
        base::Time last_active = device->last_updated_timestamp();
        bool has_high_precision = false;
        auto it = session_last_modified_times.find(device->guid());
        if (it != session_last_modified_times.end()) {
          last_active = std::max(last_active, it->second);
          // If the device has a session timestamp, it is highly precise.
          has_high_precision = true;
        }
        return DeviceWithTimestamp{device, last_active, has_high_precision};
      });
}

}  // namespace

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

std::vector<RemoteDeviceInfo> GetRemoteDeviceInfoSortedList(
    const DeviceInfoTracker& device_info_tracker,
    const base::flat_map<std::string, base::Time>& session_last_modified_times,
    base::Time min_last_active_time) {
  // Pre-calculate last active timestamps for sorting and filtering.
  std::vector<DeviceWithTimestamp> devices_with_timestamps =
      GetDevicesWithLastActiveTime(device_info_tracker.GetAllChromeDeviceInfo(),
                                   session_last_modified_times);

  // Sort the devices so the most recently active devices are first.
  std::stable_sort(
      devices_with_timestamps.begin(), devices_with_timestamps.end(),
      [](const DeviceWithTimestamp& a, const DeviceWithTimestamp& b) {
        return a.last_active > b.last_active;
      });

  const DeviceInfo* local_device = nullptr;
  std::vector<DeviceWithTimestamp> devices;
  for (const DeviceWithTimestamp& entry : devices_with_timestamps) {
    if (device_info_tracker.IsRecentLocalCacheGuid(entry.device->guid())) {
      if (!local_device) {
        local_device = entry.device;
      }
      continue;
    }
    if (entry.last_active >= min_last_active_time) {
      devices.push_back(entry);
    }
  }

  std::vector<const DeviceInfo*> raw_devices = base::ToVector(
      devices,
      [](const DeviceWithTimestamp& entry) { return entry.device.get(); });
  std::vector<std::string> device_names =
      GetDeviceDisplayNames(raw_devices, local_device);
  std::vector<RemoteDeviceInfo> remote_devices;
  remote_devices.reserve(devices.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    remote_devices.emplace_back(
        std::move(device_names[i]), devices[i].device->guid(),
        devices[i].device->form_factor(), devices[i].device->os_type(),
        devices[i].last_active, devices[i].has_high_precision);
  }
  return remote_devices;
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
