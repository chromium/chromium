// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker_impl.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
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

// Histogram name for tracking service availability at query time.
constexpr char kTrackerAvailabilityAtQueryHistogram[] =
    "Sync.CrossDevicePrefTracker.AvailabilityAtQuery";

// Maximum allowed time since a device last connected to the Sync servers before
// its entries are considered inactive and garbage collected.
constexpr base::TimeDelta kDeviceExpirationTimeout = base::Days(14);

// Helper to determine if a device is considered inactive/expired based on its
// last sync activity timestamp.
bool IsDeviceExpired(const syncer::DeviceInfo& device_info,
                     const base::Time current_time) {
  return (current_time - device_info.last_updated_timestamp()) >
         kDeviceExpirationTimeout;
}

// Helper to determine the current service availability state.
CrossDevicePrefTrackerAvailabilityAtQuery GetAvailabilityState(
    const syncer::DeviceInfoTracker* device_info_tracker,
    bool is_local_device_info_ready,
    bool is_sync_configured_for_writes) {
  if (!device_info_tracker) {
    return CrossDevicePrefTrackerAvailabilityAtQuery::kDeviceInfoTrackerMissing;
  }

  if (is_local_device_info_ready && is_sync_configured_for_writes) {
    return CrossDevicePrefTrackerAvailabilityAtQuery::kAvailable;
  }

  if (is_local_device_info_ready && !is_sync_configured_for_writes) {
    return CrossDevicePrefTrackerAvailabilityAtQuery::kSyncNotConfigured;
  }

  if (!is_local_device_info_ready && is_sync_configured_for_writes) {
    return CrossDevicePrefTrackerAvailabilityAtQuery::kLocalDeviceInfoMissing;
  }

  return CrossDevicePrefTrackerAvailabilityAtQuery::
      kSyncNotConfiguredAndLocalDeviceInfoMissing;
}

// Helper to record the Tracker's service availability metric.
void LogTrackerServiceAvailability(
    const syncer::DeviceInfoTracker* device_info_tracker,
    bool is_local_device_info_ready,
    bool is_sync_configured_for_writes) {
  CrossDevicePrefTrackerAvailabilityAtQuery availability =
      GetAvailabilityState(device_info_tracker, is_local_device_info_ready,
                           is_sync_configured_for_writes);

  base::UmaHistogramEnumeration(kTrackerAvailabilityAtQueryHistogram,
                                availability);
}

// Internal, sortable representation of a `TimestampedPrefValue`.
//
// This is a helper struct used to implement the tracker's Query API
// (e.g., `GetValues()`, `GetMostRecentValue()`) and the observer notification
// system.
//
// It enables easier processing of cross-device entries before their
// conversion to `TimestampedPrefValue` using `ToPublicApi()`.
struct TimestampedPrefValueInternal {
  // The stored preference value itself.
  base::Value value;
  // Timestamp indicating when the entry was last written to the dictionary.
  base::Time update_timestamp;
  // Timestamp indicating when the device info was last updated with the sync
  // servers.
  base::Time device_last_updated_timestamp;
  // Timestamp indicating when the tracked pref was observed to change locally.
  base::Time last_observed_change_time;
  // Sync specific unique identifier for the device.
  std::string device_sync_cache_guid;

  // Sort by most recent time.
  // Primary key: `update_timestamp`.
  // Secondary key: `device_last_updated_timestamp` (for tie-breakers).
  auto operator<=>(const TimestampedPrefValueInternal& other) const {
    return std::tie(update_timestamp, device_last_updated_timestamp) <=>
           std::tie(other.update_timestamp,
                    other.device_last_updated_timestamp);
  }

  // Converts the internal representation to the public API structure.
  TimestampedPrefValue ToPublicApi() && {
    return TimestampedPrefValue{std::move(value), last_observed_change_time,
                                std::move(device_sync_cache_guid)};
  }
};

// Helper to construct the cross-device pref name from a tracked pref name.
// This should be used internally when the input is known to be a tracked pref.
std::string GetCrossDevicePrefName(std::string_view tracked_pref_name) {
  CHECK(!tracked_pref_name.empty()) << "Tracked pref name must not be empty.";
  CHECK(!tracked_pref_name.starts_with(kCrossDevicePrefPrefix))
      << "Pref name '" << tracked_pref_name
      << "' must not start with the reserved prefix '" << kCrossDevicePrefPrefix
      << "'.";

  return base::StrCat({kCrossDevicePrefPrefix, tracked_pref_name});
}

// Helper to resolve the cross-device pref name from an input pref name, which
// can be either the tracked pref name or the cross-device pref name. This is
// used for public API methods (e.g. `GetValues()`) and validates the input
// format.
std::string ResolveCrossDevicePrefName(std::string_view pref_name) {
  if (pref_name.starts_with(kCrossDevicePrefPrefix)) {
    // It's already a cross-device pref name. Validate its structure.
    std::string_view remainder = pref_name;
    remainder.remove_prefix(strlen(kCrossDevicePrefPrefix));

    // Must not be just the prefix (e.g. "cross_device.").
    CHECK(!remainder.empty())
        << "Cross-device pref name must not be just the prefix: " << pref_name;

    // Must not have a double prefix (e.g. "cross_device.cross_device.foo").
    // This indicates a client error, as tracked prefs cannot start with the
    // prefix.
    CHECK(!remainder.starts_with(kCrossDevicePrefPrefix))
        << "Cross-device pref name must not have a double prefix: "
        << pref_name;

    return std::string(pref_name);
  }

  // If it doesn't start with the prefix, it's treated as a tracked pref name.
  CHECK(!pref_name.empty()) << "Tracked pref name must not be empty.";

  return base::StrCat({kCrossDevicePrefPrefix, pref_name});
}

// Helper to retrieve the local device's Cache GUID from
// `DeviceInfoSyncService`. Returns `std::nullopt` if the service or local
// device info is not available.
std::optional<std::string> GetLocalCacheGuid(
    syncer::DeviceInfoSyncService* service) {
  if (!service) {
    return std::nullopt;
  }

  syncer::LocalDeviceInfoProvider* local_provider =
      service->GetLocalDeviceInfoProvider();

  if (!local_provider) {
    return std::nullopt;
  }

  const syncer::DeviceInfo* local_device_info =
      local_provider->GetLocalDeviceInfo();

  if (!local_device_info) {
    return std::nullopt;
  }

  CHECK(!local_device_info->guid().empty());

  return local_device_info->guid();
}

// Helper to check if a `DeviceInfo` matches the provided filter criteria.
bool DeviceMatchesFilter(const syncer::DeviceInfo& device_info,
                         const CrossDevicePrefTracker::DeviceFilter& filter,
                         const base::Time current_time) {
  if (filter.max_sync_recency.has_value() &&
      device_info.last_updated_timestamp() <
          (current_time - filter.max_sync_recency.value())) {
    return false;
  }

  if (filter.os_type.has_value() &&
      device_info.os_type() != filter.os_type.value()) {
    return false;
  }

  if (filter.form_factor.has_value() &&
      device_info.form_factor() != filter.form_factor.value()) {
    return false;
  }

  return true;
}

// Helper to parse a cross-device pref entry into the internal
// `TimestampedPrefValueInternal` representation.
std::optional<TimestampedPrefValueInternal> ParseCrossDevicePrefEntry(
    const base::Value::Dict& cross_device_entry,
    const syncer::DeviceInfo& device_info) {
  const base::Value* value = cross_device_entry.Find(kValueKey);

  std::optional<base::Time> update_timestamp =
      base::ValueToTime(cross_device_entry.Find(kUpdateTimeKey));

  if (!value || !update_timestamp.has_value()) {
    return std::nullopt;
  }

  base::Time last_observed_change_time =
      base::ValueToTime(cross_device_entry.Find(kLastObservedChangeTimeKey))
          .value_or(base::Time());

  // Populate all fields, including the tie-breaker.
  return TimestampedPrefValueInternal{value->Clone(), update_timestamp.value(),
                                      device_info.last_updated_timestamp(),
                                      last_observed_change_time,
                                      device_info.guid()};
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

// Constructs the dictionary entry for the cross-device storage pref.
// Handles the logic for setting timestamps based on whether the change was
// observed locally.
base::Value::Dict BuildCrossDevicePrefEntry(
    const base::Value& value,
    std::optional<base::Time> observed_change_time) {
  base::Value::Dict entry;

  entry.Set(kValueKey, value.Clone());

  // Use the observed change time as the update time for consistency. If this is
  // not an observed change (e.g., initial sync), use the current time.
  const base::Time update_time =
      observed_change_time.value_or(base::Time::Now());

  entry.Set(kUpdateTimeKey, base::TimeToValue(update_time));

  // Record the observed change timestamp, but only if this is an explicit
  // local change. For initial syncs (where `observed_change_time` is null),
  // this key remains unset, even if the value changed since the last sync.
  if (observed_change_time.has_value()) {
    entry.Set(kLastObservedChangeTimeKey,
              base::TimeToValue(observed_change_time.value()));
  }

  return entry;
}

// Synchronizes the value of a local pref to the shared cross-device storage.
// If `observed_change_time` is provided, it indicates the time the change was
// observed locally; otherwise, it's considered an initial sync or an update
// triggered by readiness changes (e.g. after sign-in or `DeviceInfo` ready).
void ApplyPrefChangeToCrossDevice(
    const PrefService* tracked_pref_service,
    PrefService* profile_pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    bool is_sync_configured_for_writes,
    std::string_view tracked_pref_name,
    std::optional<base::Time> observed_change_time) {
  CHECK(tracked_pref_service);
  CHECK(profile_pref_service);
  CHECK(device_info_sync_service);

  // Do not attempt writes if Sync is not configured for the relevant types
  // (e.g. user is signed out or has disabled PREFERENCES sync). The state will
  // be refreshed when Sync becomes configured again via `OnSyncStateChanged()`.
  if (!is_sync_configured_for_writes) {
    return;
  }

  const std::optional<std::string> cache_guid =
      GetLocalCacheGuid(device_info_sync_service);

  if (!cache_guid.has_value()) {
    // Early return if the local device info (Cache GUID) isn't ready.
    // This update will be retried when `OnDeviceInfoChange()` signals
    // readiness via `HandleLocalDeviceInfoIfAvailable()`.
    return;
  }

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
    update->Remove(cache_guid.value());
    return;
  }

  const base::Value& current_value =
      tracked_pref_service->GetValue(tracked_pref_name);
  const base::Value::Dict& cross_device_dict =
      profile_pref_service->GetDict(cross_device_pref_name);
  const base::Value::Dict* existing_cross_device_entry =
      cross_device_dict.FindDict(cache_guid.value());

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

  base::Value::Dict entry =
      BuildCrossDevicePrefEntry(current_value, observed_change_time);

  ScopedDictPrefUpdate update(profile_pref_service, cross_device_pref_name);

  update->Set(cache_guid.value(), std::move(entry));
}

// Retrieves, filters, and parses all valid cross-device pref entries that
// match the provided `filter` criteria and are associated with known devices.
// Assumes `cross_device_pref_name` is valid and tracked.
std::vector<TimestampedPrefValueInternal>
GetCrossDeviceEntriesMatchingDeviceFilter(
    const PrefService& profile_pref_service,
    syncer::DeviceInfoTracker* device_info_tracker,
    std::string_view cross_device_pref_name,
    const CrossDevicePrefTracker::DeviceFilter& filter) {
  CHECK(device_info_tracker);

  const PrefService::Preference* cross_device_pref =
      profile_pref_service.FindPreference(cross_device_pref_name);

  if (!cross_device_pref ||
      cross_device_pref->GetType() != base::Value::Type::DICT) {
    return {};
  }

  const base::Value::Dict& cross_device_dict =
      profile_pref_service.GetDict(cross_device_pref_name);

  std::vector<TimestampedPrefValueInternal> matching_cross_device_entries;
  base::Time current_time = base::Time::Now();

  for (const auto [cache_guid, entry_value] : cross_device_dict) {
    const syncer::DeviceInfo* device_info =
        device_info_tracker->GetDeviceInfo(cache_guid);

    if (!device_info ||
        !DeviceMatchesFilter(*device_info, filter, current_time) ||
        !entry_value.is_dict()) {
      continue;
    }

    std::optional<TimestampedPrefValueInternal> parsed_cross_device_entry =
        ParseCrossDevicePrefEntry(entry_value.GetDict(), *device_info);

    if (parsed_cross_device_entry.has_value()) {
      matching_cross_device_entries.push_back(
          std::move(parsed_cross_device_entry.value()));
    }
  }

  return matching_cross_device_entries;
}

// Helper to extract the tracked pref name from a cross-device pref name.
std::string_view GetTrackedPrefNameFromCrossDevice(
    std::string_view cross_device_pref_name) {
  CHECK(cross_device_pref_name.starts_with(kCrossDevicePrefPrefix))
      << "Cross-device pref name must start with the reserved prefix.";

  cross_device_pref_name.remove_prefix(strlen(kCrossDevicePrefPrefix));

  return cross_device_pref_name;
}

}  // namespace

CrossDevicePrefTrackerImpl::CrossDevicePrefTrackerImpl(
    PrefService* profile_pref_service,
    PrefService* local_pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    syncer::SyncService* sync_service,
    std::unique_ptr<CrossDevicePrefProvider> pref_provider)
    : profile_pref_service_(profile_pref_service),
      local_pref_service_(local_pref_service),
      device_info_sync_service_(device_info_sync_service),
      sync_service_(sync_service),
      pref_provider_(std::move(pref_provider)) {
  CHECK(profile_pref_service_);
  CHECK(local_pref_service_);
  CHECK(device_info_sync_service_);
  // `sync_service_` can be null in tests or if Sync is disabled.
  CHECK(pref_provider_);

  is_local_device_info_ready_ =
      GetLocalCacheGuid(device_info_sync_service_).has_value();

  if (sync_service_) {
    sync_service_observation_.Observe(sync_service_);
  }

  is_sync_configured_for_writes_ = IsSyncConfiguredForWrites();

  // Initialize `DeviceInfoTracker` observation and cache known GUIDs.
  if (syncer::DeviceInfoTracker* tracker =
          device_info_sync_service_->GetDeviceInfoTracker()) {
    device_info_observation_.Observe(tracker);

    for (const syncer::DeviceInfo* device_info : GetActiveDevices()) {
      active_device_guids_.insert(device_info->guid());
    }
  }

  // Initialize the registrars with the corresponding `PrefService`.
  profile_pref_registrar_.Init(profile_pref_service_);
  local_pref_registrar_.Init(local_pref_service_);
  cross_device_pref_registrar_.Init(profile_pref_service_);

  // Initialize cross-device pref observation, validation, and caching.
  // This must happen before `StartTrackingPrefs()` so the cache is ready if
  // `ApplyPrefChangeToCrossDevice()` triggers synchronous notifications.
  StartObservingCrossDevicePrefs();

  // Initialize tracking for profile prefs (local changes).
  StartTrackingPrefs(
      pref_provider_->GetProfilePrefs(), profile_pref_registrar_,
      base::BindRepeating(
          &CrossDevicePrefTrackerImpl::OnTrackedProfilePrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // Initialize tracking for local state prefs (local changes).
  StartTrackingPrefs(
      pref_provider_->GetLocalStatePrefs(), local_pref_registrar_,
      base::BindRepeating(
          &CrossDevicePrefTrackerImpl::OnTrackedLocalStatePrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // Clean up any expired device entries from the cross-device storage.
  // This relies on `active_device_guids_`.
  GarbageCollectStaleCacheGuids();

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
  CHECK(profile_pref_service_);
  CHECK(device_info_sync_service_);

  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service_->GetDeviceInfoTracker();

  LogTrackerServiceAvailability(device_info_tracker,
                                is_local_device_info_ready_,
                                is_sync_configured_for_writes_);

  // Use `ResolveCrossDevicePrefName()` to allow either tracked or cross-device
  // pref names as input.
  std::string cross_device_pref_name = ResolveCrossDevicePrefName(pref_name);

  std::vector<TimestampedPrefValueInternal> matching_entries =
      GetCrossDeviceEntriesMatchingDeviceFilter(*profile_pref_service_,
                                                device_info_tracker,
                                                cross_device_pref_name, filter);

  std::sort(matching_entries.begin(), matching_entries.end(), std::greater<>());

  std::vector<TimestampedPrefValue> results;
  results.reserve(matching_entries.size());

  for (auto& entry : matching_entries) {
    results.push_back(std::move(entry).ToPublicApi());
  }

  return results;
}

std::optional<TimestampedPrefValue>
CrossDevicePrefTrackerImpl::GetMostRecentValue(
    std::string_view pref_name,
    const DeviceFilter& filter) const {
  CHECK(profile_pref_service_);
  CHECK(device_info_sync_service_);

  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service_->GetDeviceInfoTracker();

  LogTrackerServiceAvailability(device_info_tracker,
                                is_local_device_info_ready_,
                                is_sync_configured_for_writes_);

  // Use `ResolveCrossDevicePrefName()` to allow either tracked or cross-device
  // pref names as input.
  std::string cross_device_pref_name = ResolveCrossDevicePrefName(pref_name);

  std::vector<TimestampedPrefValueInternal> matching_entries =
      GetCrossDeviceEntriesMatchingDeviceFilter(*profile_pref_service_,
                                                device_info_tracker,
                                                cross_device_pref_name, filter);

  if (matching_entries.empty()) {
    return std::nullopt;
  }

  auto most_recent_cross_device_entry =
      std::max_element(matching_entries.begin(), matching_entries.end());

  return std::move(*most_recent_cross_device_entry).ToPublicApi();
}

void CrossDevicePrefTrackerImpl::Shutdown() {
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
  cross_device_pref_registrar_.RemoveAll();
  device_info_observation_.Reset();
  sync_service_observation_.Reset();
  pref_provider_.reset();

  profile_pref_service_ = nullptr;
  local_pref_service_ = nullptr;
  device_info_sync_service_ = nullptr;
  sync_service_ = nullptr;

#if BUILDFLAG(IS_ANDROID)
  Java_CrossDevicePrefTracker_clearNativePtr(
      base::android::AttachCurrentThread(), java_object_);
#endif  // BUILDFLAG(IS_ANDROID)
}

// `DeviceInfo` changes are relevant for several reasons:
// 1. Local `DeviceInfo` might now be available (Cache GUID readiness).
// 2. New devices might become visible (asynchronous `DeviceInfo`), or metadata
//    (`OS`/`FormFactor`) might change.
// 3. Signal for garbage collection of stale Cache GUIDs (removed or expired).
void CrossDevicePrefTrackerImpl::OnDeviceInfoChange() {
  HandleLocalDeviceInfoIfAvailable();
  HandleRemoteDeviceInfoChanges();
  GarbageCollectStaleCacheGuids();
}

void CrossDevicePrefTrackerImpl::OnStateChanged(syncer::SyncService* sync) {
  OnSyncStateChanged();
}

void CrossDevicePrefTrackerImpl::OnSyncShutdown(syncer::SyncService* sync) {
  // Unreachable because this `KeyedService` gets `Shutdown()` before the
  // `SyncService`.
  NOTREACHED();
}

void CrossDevicePrefTrackerImpl::StartObservingCrossDevicePrefs() {
  base::flat_set<std::string> cross_device_pref_names;

  // Helper lambda to validate mappings and collect cross-device pref names.
  auto process_prefs =
      [&](const base::flat_set<std::string_view>& tracked_prefs,
          PrefService* tracked_pref_service) {
        for (std::string_view tracked_pref_name : tracked_prefs) {
          // Validate the mapping early. This `CHECK`-fails if misconfigured.
          ValidatePrefMapping(tracked_pref_service, profile_pref_service_,
                              tracked_pref_name);

          cross_device_pref_names.insert(
              GetCrossDevicePrefName(tracked_pref_name));
        }
      };

  const auto& profile_prefs = pref_provider_->GetProfilePrefs();
  const auto& local_state_prefs = pref_provider_->GetLocalStatePrefs();

  process_prefs(profile_prefs, profile_pref_service_);
  process_prefs(local_state_prefs, local_pref_service_);

  // `GetProfilePrefs()` and `GetLocalStatePrefs()` should never have any
  // overlap.
  CHECK_EQ(cross_device_pref_names.size(),
           profile_prefs.size() + local_state_prefs.size())
      << "Tracked pref lists must not have any overlap.";

  for (const std::string& pref_name : cross_device_pref_names) {
    cross_device_storage_cache_[pref_name] =
        profile_pref_service_->GetDict(pref_name).Clone();

    cross_device_pref_registrar_.Add(
        pref_name, base::BindRepeating(
                       &CrossDevicePrefTrackerImpl::OnCrossDevicePrefChanged,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CrossDevicePrefTrackerImpl::StartTrackingPrefs(
    const base::flat_set<std::string_view>& pref_names,
    PrefChangeRegistrar& registrar,
    const PrefChangeRegistrar::NamedChangeAsViewCallback& callback) {
  PrefService* tracked_pref_service = registrar.prefs();
  CHECK(tracked_pref_service);

  for (std::string_view pref_name : pref_names) {
    registrar.Add(std::string(pref_name), callback);
  }

  // Perform the initial sync of the pref's current value.
  SyncOnDevicePrefsToCrossDevice(pref_names, tracked_pref_service);
}

void CrossDevicePrefTrackerImpl::SyncOnDevicePrefsToCrossDevice(
    const base::flat_set<std::string_view>& pref_names,
    PrefService* tracked_pref_service) {
  for (std::string_view pref_name : pref_names) {
    ApplyPrefChangeToCrossDevice(tracked_pref_service, profile_pref_service_,
                                 device_info_sync_service_,
                                 is_sync_configured_for_writes_, pref_name,
                                 /*observed_change_time=*/std::nullopt);
  }
}

void CrossDevicePrefTrackerImpl::OnTrackedProfilePrefChanged(
    std::string_view tracked_pref_name) {
  // This method is called by the `PrefChangeRegistrar`, meaning the pref has
  // changed locally. Record the current time as the observed change time.
  base::Time change_time = base::Time::Now();

  // Update the cross-device storage, marking this as an observed change.
  ApplyPrefChangeToCrossDevice(
      profile_pref_service_, profile_pref_service_, device_info_sync_service_,
      is_sync_configured_for_writes_, tracked_pref_name, change_time);
}

void CrossDevicePrefTrackerImpl::OnTrackedLocalStatePrefChanged(
    std::string_view tracked_pref_name) {
  // This method is called by the `PrefChangeRegistrar`, meaning the pref has
  // changed locally. Record the current time as the observed change time.
  base::Time change_time = base::Time::Now();

  // Update the cross-device storage (always a Profile pref service because it's
  // syncing), marking this as an observed change.
  ApplyPrefChangeToCrossDevice(
      local_pref_service_, profile_pref_service_, device_info_sync_service_,
      is_sync_configured_for_writes_, tracked_pref_name, change_time);
}

void CrossDevicePrefTrackerImpl::OnCrossDevicePrefChanged(
    std::string_view cross_device_pref_name_view) {
  CHECK(profile_pref_service_);
  std::string pref_name(cross_device_pref_name_view);

  const base::Value::Dict& new_dict = profile_pref_service_->GetDict(pref_name);

  auto cache_it = cross_device_storage_cache_.find(pref_name);

  // This should not happen as the cache is initialized with all tracked prefs.
  CHECK(cache_it != cross_device_storage_cache_.end());

  const base::Value::Dict& old_dict = cache_it->second;

  // Optimization: Check if the dictionaries are identical before proceeding.
  if (old_dict == new_dict) {
    return;
  }

  // Process updates and notify observers only for remote changes.
  // Local updates (triggered synchronously by `ApplyPrefChangeToCrossDevice`)
  // are identified and suppressed within this call by checking the Cache GUID.
  ProcessRemoteUpdates(pref_name, old_dict, new_dict);

  // Update the cache for the next iteration. This must happen regardless of
  // whether the change was local or remote.
  cache_it->second = new_dict.Clone();
}

void CrossDevicePrefTrackerImpl::ProcessRemoteUpdates(
    const std::string& cross_device_pref_name,
    const base::Value::Dict& old_dict,
    const base::Value::Dict& new_dict) {
  CHECK(device_info_sync_service_);
  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service_->GetDeviceInfoTracker();

  if (!device_info_tracker) {
    return;
  }

  const std::optional<std::string> local_cache_guid =
      GetLocalCacheGuid(device_info_sync_service_);

  // Iterate over the new dictionary to find added or updated entries.
  for (const auto [cache_guid, new_entry_value] : new_dict) {
    // Skip updates from the local device first to short-circuit.
    if (local_cache_guid.has_value() &&
        cache_guid == local_cache_guid.value()) {
      continue;
    }

    const base::Value::Dict* new_entry = new_entry_value.GetIfDict();

    // Skip malformed entries.
    if (!new_entry) {
      continue;
    }

    const base::Value::Dict* old_entry = old_dict.FindDict(cache_guid);

    // Check if the entry is new or updated by comparing the dictionaries.
    if (!old_entry || *old_entry != *new_entry) {
      // Remote change detected.

      const syncer::DeviceInfo* device_info =
          device_info_tracker->GetDeviceInfo(cache_guid);

      if (!device_info) {
        // Device info not available yet. Skip notification for now.
        // It will be handled in `OnDeviceInfoChange()` when the corresponding
        // `DeviceInfo` appears.
        continue;
      }

      NotifyRemotePrefChanged(cross_device_pref_name, new_entry, *device_info);
    }
  }

  // Iterate over the old dictionary to find deleted entries.
  for (const auto [cache_guid, old_entry_value] : old_dict) {
    // Skip local deletions.
    if (local_cache_guid.has_value() &&
        cache_guid == local_cache_guid.value()) {
      continue;
    }

    if (new_dict.contains(cache_guid)) {
      continue;
    }

    // Remote deletion detected.

    const syncer::DeviceInfo* device_info =
        device_info_tracker->GetDeviceInfo(cache_guid);

    if (!device_info) {
      // `DeviceInfo` for the device that previously held the pref is
      // unavailable; cannot notify without the source device's metadata, so
      // don't notify observers.
      continue;
    }

    // Notify observers of the removal. Passing nullptr for the entry signifies
    // that the preference is no longer set on the remote device.
    NotifyRemotePrefChanged(cross_device_pref_name, /*entry=*/nullptr,
                            *device_info);
  }
}

void CrossDevicePrefTrackerImpl::NotifyRemotePrefChanged(
    const std::string& cross_device_pref_name,
    const base::Value::Dict* entry,
    const syncer::DeviceInfo& remote_device_info) {
  // Default constructed value signifies deletion (null value and null time).
  TimestampedPrefValue timestamped_value;

  if (entry) {
    std::optional<TimestampedPrefValueInternal> internal_value =
        ParseCrossDevicePrefEntry(*entry, remote_device_info);

    if (!internal_value.has_value()) {
      // Entry is invalid, skip notifying observers.
      return;
    }

    timestamped_value = std::move(internal_value.value()).ToPublicApi();
  }

  // If `entry` is null, it's a deletion, and `timestamped_value` remains
  // default constructed.

  std::string_view tracked_pref_name =
      GetTrackedPrefNameFromCrossDevice(cross_device_pref_name);

  for (auto& observer : observers_) {
    observer.OnRemotePrefChanged(tracked_pref_name, timestamped_value,
                                 remote_device_info);
  }

  // TODO(crbug.com/442902926): Notify Java side of updates.
}

void CrossDevicePrefTrackerImpl::HandleLocalDeviceInfoIfAvailable() {
  if (is_local_device_info_ready_) {
    return;
  }

  if (!GetLocalCacheGuid(device_info_sync_service_).has_value()) {
    return;
  }

  is_local_device_info_ready_ = true;

  // Now that the Cache GUID is available, push the initial state of all
  // tracked prefs. This is NOT considered an observed change.
  // This uses the cached `is_sync_configured_for_writes_`.
  SyncAllOnDevicePrefsToCrossDevice();
}

void CrossDevicePrefTrackerImpl::OnSyncStateChanged() {
  bool was_configured = is_sync_configured_for_writes_;
  is_sync_configured_for_writes_ = IsSyncConfiguredForWrites();

  if (!was_configured && is_sync_configured_for_writes_) {
    // If Sync just became configured (e.g. user signed in or enabled Prefs
    // sync), refresh all pref states to ensure the cross-device storage
    // reflects the latest local values, as writes were blocked while
    // unconfigured.
    SyncAllOnDevicePrefsToCrossDevice();
  }
}

bool CrossDevicePrefTrackerImpl::IsSyncConfiguredForWrites() const {
  if (!sync_service_) {
    return false;
  }

  // Check if the user has selected PREFERENCES for syncing. Rely on
  // `GetUserSettings()->GetSelectedTypes()` instead of `GetActiveDataTypes()`
  // so that writes are allowed even if Sync is temporarily paused or inactive
  // (e.g. network issues).
  if (!sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPreferences)) {
    return false;
  }

  return true;
}

void CrossDevicePrefTrackerImpl::SyncAllOnDevicePrefsToCrossDevice() {
  // Push the current state of all tracked prefs. This is NOT considered an
  // observed change.
  SyncOnDevicePrefsToCrossDevice(pref_provider_->GetProfilePrefs(),
                                 profile_pref_service_);
  SyncOnDevicePrefsToCrossDevice(pref_provider_->GetLocalStatePrefs(),
                                 local_pref_service_);
}

void CrossDevicePrefTrackerImpl::HandleRemoteDeviceInfoChanges() {
  base::flat_set<std::string> current_active_guids;

  std::vector<const syncer::DeviceInfo*> new_or_reactivated_devices;

  const std::optional<std::string> local_cache_guid =
      GetLocalCacheGuid(device_info_sync_service_);

  for (const syncer::DeviceInfo* device_info : GetActiveDevices()) {
    const std::string& guid = device_info->guid();

    current_active_guids.insert(guid);

    // Don't notify for the local device itself.
    if (local_cache_guid == guid) {
      continue;
    }

    // A device is considered new or reactivated if it's currently active but
    // wasn't in the previous set of known GUIDs.
    if (!active_device_guids_.contains(guid)) {
      new_or_reactivated_devices.push_back(device_info);
    }
  }

  // If there are new or reactivated devices, notify observers about their
  // existing pref values. This handles the case where pref data arrived via
  // Sync before the corresponding `DeviceInfo`.
  if (!new_or_reactivated_devices.empty()) {
    NotifyObserversOfExistingPrefsForNewDevices(new_or_reactivated_devices);
  }

  // Update the cached set of known devices for the next iteration.
  // This handles device removals and expirations as well.
  active_device_guids_ = std::move(current_active_guids);
}

void CrossDevicePrefTrackerImpl::NotifyObserversOfExistingPrefsForNewDevices(
    const std::vector<const syncer::DeviceInfo*>& new_devices) {
  for (const auto& [pref_name, dict_value] : cross_device_storage_cache_) {
    for (const syncer::DeviceInfo* device_info : new_devices) {
      const base::Value::Dict* entry = dict_value.FindDict(device_info->guid());

      if (entry) {
        // Found an existing pref value for the newly available device.
        NotifyRemotePrefChanged(pref_name, entry, *device_info);
      }
    }
  }
}

void CrossDevicePrefTrackerImpl::GarbageCollectStaleCacheGuids() {
  CHECK(profile_pref_service_);

  // Rely on `active_device_guids_` which contains the set of all currently
  // active (non-expired) Cache GUIDs known by `DeviceInfoTracker`.
  const base::flat_set<std::string>& active_guids = active_device_guids_;

  // Identify stale entries without modifying the PrefService or cache.
  base::flat_map<std::string, std::vector<std::string>> entries_to_remove;

  for (const auto& [cross_device_pref_name, cache_dict] :
       cross_device_storage_cache_) {
    std::vector<std::string> guids_to_remove;

    for (const auto [guid, value] : cache_dict) {
      if (!active_guids.contains(guid)) {
        guids_to_remove.push_back(guid);
      }
    }

    if (!guids_to_remove.empty()) {
      entries_to_remove[cross_device_pref_name] = std::move(guids_to_remove);
    }
  }

  // Remove the stale entries. It is now safe for the cache to be updated.
  for (const auto& [cross_device_pref_name, guids_to_remove] :
       entries_to_remove) {
    ScopedDictPrefUpdate update(profile_pref_service_, cross_device_pref_name);

    for (const std::string& guid : guids_to_remove) {
      bool removed = update->Remove(guid);
      CHECK(removed);
    }
  }
}

std::vector<const syncer::DeviceInfo*>
CrossDevicePrefTrackerImpl::GetActiveDevices() const {
  CHECK(device_info_sync_service_);

  syncer::DeviceInfoTracker* tracker =
      device_info_sync_service_->GetDeviceInfoTracker();

  if (!tracker) {
    return {};
  }

  const std::optional<std::string> local_cache_guid =
      GetLocalCacheGuid(device_info_sync_service_);

  const base::Time current_time = base::Time::Now();

  std::vector<const syncer::DeviceInfo*> active_devices;

  for (const auto* device_info : tracker->GetAllDeviceInfo()) {
    const bool is_local_device =
        local_cache_guid.has_value() &&
        device_info->guid() == local_cache_guid.value();

    // The local device is always active. Remote devices are active if not
    // expired.
    if (is_local_device || !IsDeviceExpired(*device_info, current_time)) {
      active_devices.push_back(device_info);
    }
  }

  return active_devices;
}

#if BUILDFLAG(IS_ANDROID)
namespace {

// Helper to convert OsType and FormFactor from Java ints to optional C++ enums.
CrossDevicePrefTracker::DeviceFilter ToDeviceFilter(
    std::optional<int> os_type,
    std::optional<int> form_factor,
    std::optional<jlong> max_sync_recency_microseconds) {
  CrossDevicePrefTracker::DeviceFilter filter;
  if (os_type.has_value()) {
    filter.os_type = static_cast<syncer::DeviceInfo::OsType>(os_type.value());
  }
  if (form_factor.has_value()) {
    filter.form_factor =
        static_cast<syncer::DeviceInfo::FormFactor>(form_factor.value());
  }
  if (max_sync_recency_microseconds.has_value()) {
    filter.max_sync_recency =
        base::Microseconds(max_sync_recency_microseconds.value());
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
    std::optional<int> form_factor,
    std::optional<jlong> max_sync_recency_microseconds) const {
  std::vector<ScopedJavaLocalRef<jobject>> result;
  std::vector<TimestampedPrefValue> timestamped_pref_values = GetValues(
      base::android::ConvertJavaStringToUTF8(env, pref_name),
      ToDeviceFilter(os_type, form_factor, max_sync_recency_microseconds));
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
    std::optional<int> form_factor,
    std::optional<jlong> max_sync_recency_microseconds) const {
  std::optional<TimestampedPrefValue> timestamped_pref_value =
      GetMostRecentValue(
          base::android::ConvertJavaStringToUTF8(env, pref_name),
          ToDeviceFilter(os_type, form_factor, max_sync_recency_microseconds));
  if (!timestamped_pref_value.has_value()) {
    return nullptr;
  }
  TimestampedPrefValueBridge bridge(timestamped_pref_value.value());
  return bridge.GetJavaObject();
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace sync_preferences

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(CrossDevicePrefTracker)
#endif
