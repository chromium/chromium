// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_REMOTE_DEVICE_INFO_H_
#define COMPONENTS_SYNC_DEVICE_INFO_REMOTE_DEVICE_INFO_H_

#include <string>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/sync_device_info/device_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/jni_zero/jni_zero.h"
#endif

namespace syncer {

// Remote device information for generating cross-device UIs.
struct RemoteDeviceInfo {
  RemoteDeviceInfo();
  RemoteDeviceInfo(std::string device_name,
                   std::string cache_guid,
                   DeviceInfo::FormFactor form_factor,
                   DeviceInfo::OsType os_type,
                   base::Time last_updated_timestamp,
                   bool has_high_precision_timestamp);
  RemoteDeviceInfo(const RemoteDeviceInfo& other);
  ~RemoteDeviceInfo();

  bool operator==(const RemoteDeviceInfo& rhs) const;

  // Returns a localized string representing the time since the device was last
  // updated. When `has_high_precision_timestamp` is true, sub-day durations
  // are formatted as "Active now" (< 1 minute) or "Active X minutes/hours ago"
  // (< 1 day); when false, only day-granularity durations ("Active today",
  // "Active X days ago") are used.
  static std::u16string GetLastActiveTimeForDisplay(
      base::Time last_updated_timestamp,
      bool has_high_precision_timestamp);
  std::u16string GetLastActiveTimeForDisplay() const;

  // Device display name.
  std::string device_name;
  // Device guid.
  std::string cache_guid;
  // Device Form Factor.
  DeviceInfo::FormFactor form_factor = DeviceInfo::FormFactor::kUnknown;
  // Device OS Type.
  DeviceInfo::OsType os_type = DeviceInfo::OsType::kUnknown;
  // Last updated timestamp.
  base::Time last_updated_timestamp;
  // Whether the device timestamp is highly precise (e.g. from sessions sync)
  // rather than just day-granularity.
  bool has_high_precision_timestamp = false;
};

}  // namespace syncer

#if BUILDFLAG(IS_ANDROID)
namespace jni_zero {

template <>
ScopedJavaLocalRef<jobject> ToJniType<syncer::RemoteDeviceInfo>(
    JNIEnv* env,
    const syncer::RemoteDeviceInfo& input);

}  // namespace jni_zero
#endif

#endif  // COMPONENTS_SYNC_DEVICE_INFO_REMOTE_DEVICE_INFO_H_
