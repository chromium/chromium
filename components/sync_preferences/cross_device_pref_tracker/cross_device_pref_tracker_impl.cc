// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker_impl.h"

#include "base/check.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info_sync_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "components/sync_preferences/cross_device_pref_tracker/android/timestamped_pref_value_bridge_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/sync_preferences/cross_device_pref_tracker/android/jni_headers/CrossDevicePrefTracker_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
using base::android::ScopedJavaLocalRef;
#endif  // BUILDFLAG(IS_ANDROID)

namespace sync_preferences {

CrossDevicePrefTrackerImpl::CrossDevicePrefTrackerImpl(
    PrefService* profile_pref_service,
    PrefService* local_pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service)
    : profile_pref_service_(profile_pref_service),
      local_pref_service_(local_pref_service),
      device_info_sync_service_(device_info_sync_service) {
  CHECK(profile_pref_service_);
  CHECK(local_pref_service_);
  CHECK(device_info_sync_service_);

  // Initialize the registrars with the corresponding `PrefService`.
  profile_pref_registrar_.Init(profile_pref_service_);
  local_pref_registrar_.Init(local_pref_service_);

  // Start observing the `DeviceInfoTracker`. This is required to map remote
  // Cache GUIDs to device metadata (OS type, form factor).
  if (syncer::DeviceInfoTracker* tracker =
          device_info_sync_service_->GetDeviceInfoTracker()) {
    device_info_observation_.Observe(tracker);
  }

  // TODO(crbug.com/441330511): Initialize tracking for specific Prefs based on
  // the static maps (will be done in a follow-up CL).

#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_CrossDevicePrefTracker_Constructor(
      env, reinterpret_cast<intptr_t>(this)));
#endif  // BUILDFLAG(IS_ANDROID)
}

CrossDevicePrefTrackerImpl::~CrossDevicePrefTrackerImpl() {
  // `Shutdown()` should have been called by the `KeyedService` infrastructure.
  CHECK(!profile_pref_service_);
  CHECK(!local_pref_service_);
  CHECK(!device_info_sync_service_);
}

void CrossDevicePrefTrackerImpl::AddObserver(
    CrossDevicePrefTracker::Observer* observer) {
  observers_.AddObserver(observer);
}

void CrossDevicePrefTrackerImpl::RemoveObserver(
    CrossDevicePrefTracker::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<TimestampedPrefValue> CrossDevicePrefTrackerImpl::GetValues(
    std::string_view pref_name,
    const DeviceFilter& filter) const {
  // TODO(crbug.com/441330219): Implement the Query API.

  return {};
}

std::optional<TimestampedPrefValue>
CrossDevicePrefTrackerImpl::GetMostRecentValue(
    std::string_view pref_name,
    const DeviceFilter& filter) const {
  // TODO(crbug.com/441330219): Implement the Query API.

  return std::nullopt;
}

void CrossDevicePrefTrackerImpl::Shutdown() {
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
  device_info_observation_.Reset();

  profile_pref_service_ = nullptr;
  local_pref_service_ = nullptr;
  device_info_sync_service_ = nullptr;

#if BUILDFLAG(IS_ANDROID)
  Java_CrossDevicePrefTracker_clearNativePtr(
      base::android::AttachCurrentThread(), java_object_);
#endif  // BUILDFLAG(IS_ANDROID)
}

void CrossDevicePrefTrackerImpl::OnDeviceInfoChange() {
  // `DeviceInfo` changes are relevant for two main reasons:
  // 1. Metadata (OS/FormFactor) might change, affecting query results or
  //    observer notifications.
  // 2. This is the signal to perform garbage collection of stale Cache GUIDs
  //    from the syncable dictionary Prefs (as noted in the design doc).

  // TODO(crbug.com/441330511): Implement garbage collection and handle metadata
  // updates.
  // TODO(crbug.com/442902926): Notify Java side of updates.
}

#if BUILDFLAG(IS_ANDROID)
namespace {

// Helper to convert OsType and FormFactor from Java ints to optional C++ enums.
CrossDevicePrefTracker::DeviceFilter ToDeviceFilter(
    std::optional<int> os_type,
    std::optional<int> form_factor) {
  CrossDevicePrefTracker::DeviceFilter filter;
  if (os_type.has_value()) {
    filter.os_type = static_cast<syncer::DeviceInfo::OsType>(os_type.value());
  }
  if (form_factor.has_value()) {
    filter.form_factor =
        static_cast<syncer::DeviceInfo::FormFactor>(form_factor.value());
  }
  return filter;
}

}  // namespace

// Return the Java object that allows access to the CrossDevicePrefTracker.
ScopedJavaLocalRef<jobject> CrossDevicePrefTrackerImpl::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_object_);
}

// Java versions of query methods.
ScopedJavaLocalRef<jobjectArray> CrossDevicePrefTrackerImpl::GetValues(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& pref_name,
    std::optional<int> os_type,
    std::optional<int> form_factor) const {
  std::vector<ScopedJavaLocalRef<jobject>> result;
  std::vector<TimestampedPrefValue> timestamped_pref_values =
      GetValues(base::android::ConvertJavaStringToUTF8(env, pref_name),
                ToDeviceFilter(os_type, form_factor));
  for (const auto& timestamped_pref_value : timestamped_pref_values) {
    TimestampedPrefValueBridge bridge(timestamped_pref_value);
    result.push_back(bridge.GetJavaObject());
  }
  return base::android::ToJavaArrayOfObjects(env, result);
}

ScopedJavaLocalRef<jobject> CrossDevicePrefTrackerImpl::GetMostRecentValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& pref_name,
    std::optional<int> os_type,
    std::optional<int> form_factor) const {
  std::optional<TimestampedPrefValue> timestamped_pref_value =
      GetMostRecentValue(base::android::ConvertJavaStringToUTF8(env, pref_name),
                         ToDeviceFilter(os_type, form_factor));
  if (!timestamped_pref_value.has_value()) {
    return nullptr;
  }
  TimestampedPrefValueBridge bridge(timestamped_pref_value.value());
  return bridge.GetJavaObject();
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace sync_preferences
