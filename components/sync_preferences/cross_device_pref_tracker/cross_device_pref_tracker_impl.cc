// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"

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

namespace {

// Keys used for the cross-device syncable storage dictionary pref. For more
// details on the design, see go/cross-device-pref-tracker.

// Prefix for all cross-device dictionary pref names.
constexpr char kCrossDevicePrefPrefix[] = "cross_device.";

// Key used in the per-device dictionary that holds the actual pref value.
constexpr char kValueKey[] = "value";

// Key used in the per-device dictionary that stores the timestamp of the
// last write.
constexpr char kUpdateTimeKey[] = "update_time";

// Key used in the per-device dictionary that stores the timestamp of the
// last observed local change via `PrefChangeRegistrar`. This allows clients to
// distinguish between initial synchronization (which might reflect default
// values) and explicit local modifications.
constexpr char kLastObservedChangeTimeKey[] = "last_observed_change_time";

// Helper to construct the cross-device pref name from a tracked pref name.
std::string GetCrossDevicePrefName(std::string_view tracked_pref_name) {
  return base::StrCat({kCrossDevicePrefPrefix, tracked_pref_name});
}

// Enforces the integrity of a pref mapping at startup to prevent runtime
// errors. It verifies that both the tracked and cross-device prefs are
// registered. An invalid mapping indicates a developer error and will
// CHECK-fail.
void ValidatePrefMapping(const PrefService* tracked_pref_service,
                         const PrefService* profile_pref_service,
                         std::string_view tracked_pref_name) {
  CHECK(tracked_pref_service);
  CHECK(profile_pref_service);

  const PrefService::Preference* tracked_pref =
      tracked_pref_service->FindPreference(tracked_pref_name);
  CHECK(tracked_pref) << "Tracked pref '" << tracked_pref_name
                      << "' is not registered.";

  std::string cross_device_pref_name =
      GetCrossDevicePrefName(tracked_pref_name);
  const PrefService::Preference* cross_device_pref =
      profile_pref_service->FindPreference(cross_device_pref_name);

  CHECK(cross_device_pref) << "Cross-device pref '" << cross_device_pref_name
                           << "' is not registered.";
  CHECK(cross_device_pref->GetType() == base::Value::Type::DICT)
      << "Cross-device pref '" << cross_device_pref_name
      << "' must be a dictionary.";
}

// Synchronizes the value of a local pref to the shared cross-device storage.
// If `observed_change_time` is provided, it indicates the time the change was
// observed locally; otherwise, it's considered an initial sync or an update
// triggered by `OnDeviceInfoChange` (e.g. after sign-in).
void ApplyPrefChangeToCrossDevice(
    const PrefService* tracked_pref_service,
    PrefService* profile_pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    std::string_view tracked_pref_name,
    std::optional<base::Time> observed_change_time) {
  CHECK(tracked_pref_service);
  CHECK(profile_pref_service);
  CHECK(device_info_sync_service);

  // TODO(crbug.com/444632366): Check if Sync is enabled before attempting to
  // write to the cross-device dictionary.
  syncer::LocalDeviceInfoProvider* local_provider =
      device_info_sync_service->GetLocalDeviceInfoProvider();
  if (!local_provider) {
    return;
  }

  const syncer::DeviceInfo* local_device_info =
      local_provider->GetLocalDeviceInfo();
  if (!local_device_info) {
    // Early return if the local device info (Cache GUID) isn't ready.
    // This update will be retried when `OnDeviceInfoChange()` signals
    // readiness.
    return;
  }

  const std::string& cache_guid = local_device_info->guid();
  CHECK(!cache_guid.empty());

  const PrefService::Preference* tracked_pref =
      tracked_pref_service->FindPreference(tracked_pref_name);
  CHECK(tracked_pref);

  std::string cross_device_pref_name =
      GetCrossDevicePrefName(tracked_pref_name);

  // If the current value is the default, it should not be propagated. Instead,
  // the corresponding entry in the cross-device dictionary should be cleared to
  // signal that this device no longer has a value set by the user.
  if (tracked_pref->IsDefaultValue()) {
    ScopedDictPrefUpdate update(profile_pref_service, cross_device_pref_name);
    update->Remove(cache_guid);
    return;
  }

  const base::Value& current_value =
      tracked_pref_service->GetValue(tracked_pref_name);
  const base::Value::Dict& cross_device_dict =
      profile_pref_service->GetDict(cross_device_pref_name);
  const base::Value::Dict* existing_cross_device_entry =
      cross_device_dict.FindDict(cache_guid);

  // Optimization: Minimize writes to the syncable pref to reduce sync traffic,
  // but ensure observed changes always update timestamps for recency.
  if (existing_cross_device_entry) {
    const base::Value* existing_cross_device_value =
        existing_cross_device_entry->Find(kValueKey);
    bool value_matches = (existing_cross_device_value &&
                          *existing_cross_device_value == current_value);

    if (value_matches && !observed_change_time.has_value()) {
      // Skip update if the value is the same AND this is not an observed
      // change (e.g., initial sync or refresh). This correctly preserves the
      // existing entry, including any existing timestamps, without requiring
      // a write.
      return;
    }
  }

  // If the value changed, it's an observed change (even if value is the same),
  // or if no entry exists, the update must proceed.

  base::Value::Dict entry;
  entry.Set(kValueKey, current_value.Clone());

  // Always update the timestamp indicating when this write occurred. This is
  // required for recency sorting in the Query API.
  // Use the observed time if available for consistency, otherwise use the
  // current time.
  base::Time update_time = observed_change_time.value_or(base::Time::Now());
  entry.Set(kUpdateTimeKey, base::TimeToValue(update_time));

  // Record the observed change timestamp, but only if this is an explicit
  // local change. For initial syncs (where `observed_change_time` is null),
  // this key remains unset.
  if (observed_change_time.has_value()) {
    entry.Set(kLastObservedChangeTimeKey, base::TimeToValue(update_time));
  }

  ScopedDictPrefUpdate update(profile_pref_service, cross_device_pref_name);
  update->Set(cache_guid, std::move(entry));
}

}  // namespace

CrossDevicePrefTrackerImpl::CrossDevicePrefTrackerImpl(
    PrefService* profile_pref_service,
    PrefService* local_pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    std::unique_ptr<CrossDevicePrefProvider> pref_provider)
    : profile_pref_service_(profile_pref_service),
      local_pref_service_(local_pref_service),
      device_info_sync_service_(device_info_sync_service),
      pref_provider_(std::move(pref_provider)) {
  CHECK(profile_pref_service_);
  CHECK(local_pref_service_);
  CHECK(device_info_sync_service_);
  CHECK(pref_provider_);

  syncer::LocalDeviceInfoProvider* local_provider =
      device_info_sync_service_->GetLocalDeviceInfoProvider();
  is_local_device_info_ready_ =
      (local_provider && local_provider->GetLocalDeviceInfo());

  // Initialize the registrars with the corresponding `PrefService`.
  profile_pref_registrar_.Init(profile_pref_service_);
  local_pref_registrar_.Init(local_pref_service_);

  // Initialize tracking for profile prefs. Tracked and cross-device storage are
  // both in the profile pref service.
  StartTrackingPrefs(
      pref_provider_->GetProfilePrefs(), profile_pref_service_,
      profile_pref_registrar_,
      base::BindRepeating(
          &CrossDevicePrefTrackerImpl::OnTrackedProfilePrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // Initialize tracking for local state prefs. Tracked in local state, but
  // cross-device storage is in the profile pref service (as it must be
  // syncable).
  StartTrackingPrefs(
      pref_provider_->GetLocalStatePrefs(), local_pref_service_,
      local_pref_registrar_,
      base::BindRepeating(
          &CrossDevicePrefTrackerImpl::OnTrackedLocalStatePrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // Start observing the `DeviceInfoTracker`. This is required to map remote
  // Cache GUIDs to device metadata and to handle delayed initialization.
  if (syncer::DeviceInfoTracker* tracker =
          device_info_sync_service_->GetDeviceInfoTracker()) {
    device_info_observation_.Observe(tracker);
  }

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
  pref_provider_.reset();

  profile_pref_service_ = nullptr;
  local_pref_service_ = nullptr;
  device_info_sync_service_ = nullptr;

#if BUILDFLAG(IS_ANDROID)
  Java_CrossDevicePrefTracker_clearNativePtr(
      base::android::AttachCurrentThread(), java_object_);
#endif  // BUILDFLAG(IS_ANDROID)
}

// `DeviceInfo` changes are relevant for several reasons:
// 1. Local `DeviceInfo` might now be available. If initialization happened
//    before the Cache GUID was ready, the initial state of all tracked prefs
//    needs to be pushed now.
// 2. Metadata (OS/FormFactor) might change, affecting query results or
//    observer notifications.
// 3. This is the signal to perform garbage collection of stale Cache GUIDs
//    from the syncable dictionary prefs.
void CrossDevicePrefTrackerImpl::OnDeviceInfoChange() {
  CHECK(device_info_sync_service_);

  if (!is_local_device_info_ready_) {
    syncer::LocalDeviceInfoProvider* local_provider =
        device_info_sync_service_->GetLocalDeviceInfoProvider();
    if (local_provider && local_provider->GetLocalDeviceInfo()) {
      is_local_device_info_ready_ = true;

      // Now that the Cache GUID is available, push the initial state of all
      // tracked prefs. This is NOT considered an observed change.
      for (std::string_view pref_name : pref_provider_->GetProfilePrefs()) {
        ApplyPrefChangeToCrossDevice(
            profile_pref_service_, profile_pref_service_,
            device_info_sync_service_, pref_name, std::nullopt);
      }

      for (std::string_view pref_name : pref_provider_->GetLocalStatePrefs()) {
        ApplyPrefChangeToCrossDevice(local_pref_service_, profile_pref_service_,
                                     device_info_sync_service_, pref_name,
                                     std::nullopt);
      }
    }
  }

  // TODO(crbug.com/441330511): Implement garbage collection and handle metadata
  // updates.
  // TODO(crbug.com/442902926): Notify Java side of updates.
}

void CrossDevicePrefTrackerImpl::StartTrackingPrefs(
    const base::flat_set<std::string_view>& pref_names,
    PrefService* tracked_pref_service,
    PrefChangeRegistrar& registrar,
    const PrefChangeRegistrar::NamedChangeAsViewCallback& callback) {
  for (std::string_view pref_name : pref_names) {
    ValidatePrefMapping(tracked_pref_service, profile_pref_service_, pref_name);

    registrar.Add(std::string(pref_name), callback);

    // Perform the initial sync of the pref's current value. This is NOT
    // considered an observed change.
    ApplyPrefChangeToCrossDevice(tracked_pref_service, profile_pref_service_,
                                 device_info_sync_service_, pref_name,
                                 std::nullopt);
  }
}

void CrossDevicePrefTrackerImpl::OnTrackedProfilePrefChanged(
    std::string_view tracked_pref_name) {
  CHECK(device_info_sync_service_);

  // This method is called by the `PrefChangeRegistrar`, meaning the pref has
  // changed locally. Record the current time as the observed change time.
  base::Time change_time = base::Time::Now();

  // Update the cross-device storage, marking this as an observed change.
  ApplyPrefChangeToCrossDevice(profile_pref_service_, profile_pref_service_,
                               device_info_sync_service_, tracked_pref_name,
                               change_time);
}

void CrossDevicePrefTrackerImpl::OnTrackedLocalStatePrefChanged(
    std::string_view tracked_pref_name) {
  CHECK(device_info_sync_service_);

  // This method is called by the `PrefChangeRegistrar`, meaning the pref has
  // changed locally. Record the current time as the observed change time.
  base::Time change_time = base::Time::Now();

  // Update the cross-device storage (always a Profile pref service because it's
  // syncing), marking this as an observed change.
  ApplyPrefChangeToCrossDevice(local_pref_service_, profile_pref_service_,
                               device_info_sync_service_, tracked_pref_name,
                               change_time);
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
