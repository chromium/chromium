// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"

#include <stddef.h>

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace {

std::string GetPartitionKey(const base::FilePath& relative_path) {
  // Create a partition_key string with no '.'s in it.
  const base::FilePath::StringType& path = relative_path.value();
  // Prepend "x" to prevent an unlikely collision with an old
  // partition key (which contained only [0-9]).
  return "x" +
         base::HexEncode(
             path.c_str(),
             path.size() * sizeof(base::FilePath::StringType::value_type));
}

const char kZoomLevelKey[] = "zoom_level";
const char kLastModifiedPath[] = "last_modified";

// Extract a timestamp from |dictionary[kLastModifiedPath]|.
// Will return base::Time() if no timestamp exists.
base::Time GetTimeStamp(const base::Value::Dict& dictionary) {
  std::string timestamp_str;
  const std::string* timestamp_str_ptr =
      dictionary.FindString(kLastModifiedPath);
  if (timestamp_str_ptr)
    timestamp_str = *timestamp_str_ptr;
  int64_t timestamp = 0;
  base::StringToInt64(timestamp_str, &timestamp);
  base::Time last_modified = base::Time::FromInternalValue(timestamp);
  return last_modified;
}

}  // namespace

ChromeZoomLevelPrefs::ChromeZoomLevelPrefs(
    PrefService* pref_service,
    const base::FilePath& profile_path,
    const base::FilePath& partition_path,
    base::WeakPtr<zoom::ZoomEventManager> zoom_event_manager)
    : pref_service_(pref_service),
      zoom_event_manager_(zoom_event_manager),
      host_zoom_map_(nullptr) {
  DCHECK(pref_service_);

  DCHECK(!partition_path.empty());
  DCHECK((partition_path == profile_path) ||
         profile_path.IsParent(partition_path));
  base::FilePath partition_relative_path;
  profile_path.AppendRelativePath(partition_path, &partition_relative_path);
  partition_key_ = GetPartitionKey(partition_relative_path);
}

ChromeZoomLevelPrefs::~ChromeZoomLevelPrefs() {}

std::string ChromeZoomLevelPrefs::GetPartitionKeyForTesting(
    const base::FilePath& relative_path) {
  return GetPartitionKey(relative_path);
}

void ChromeZoomLevelPrefs::SetDefaultZoomLevelPref(double level) {
  if (blink::ZoomValuesEqual(level, host_zoom_map_->GetDefaultZoomLevel())) {
    return;
  }

  ScopedDictPrefUpdate update(pref_service_, prefs::kPartitionDefaultZoomLevel);
  update->Set(partition_key_, level);
  // For unregistered paths, OnDefaultZoomLevelChanged won't be called, so
  // set this manually.
  host_zoom_map_->SetDefaultZoomLevel(level);
  default_zoom_changed_callbacks_.Notify();
  if (zoom_event_manager_)
    zoom_event_manager_->OnDefaultZoomLevelChanged();
}

double ChromeZoomLevelPrefs::GetDefaultZoomLevelPref() const {
  const base::Value::Dict& default_zoom_level_dictionary =
      pref_service_->GetDict(prefs::kPartitionDefaultZoomLevel);
  return default_zoom_level_dictionary.FindDouble(partition_key_).value_or(0.0);
}

base::CallbackListSubscription
ChromeZoomLevelPrefs::RegisterDefaultZoomLevelCallback(
    base::RepeatingClosure callback) {
  return default_zoom_changed_callbacks_.Add(std::move(callback));
}

void ChromeZoomLevelPrefs::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  // If there's a manager to aggregate ZoomLevelChanged events, pass this event
  // along. Since we already hold a subscription to our associated HostZoomMap,
  // we don't need to create a separate subscription for this.
  if (zoom_event_manager_)
    zoom_event_manager_->OnZoomLevelChanged(change);

  if (change.mode != content::HostZoomMap::ZOOM_CHANGED_FOR_HOST)
    return;
  double level = change.zoom_level;
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kPartitionPerHostZoomLevels);
  base::Value::Dict& host_zoom_dictionaries = update.Get();

  bool modification_is_removal =
      blink::ZoomValuesEqual(level, host_zoom_map_->GetDefaultZoomLevel());

  base::Value::Dict* host_zoom_dictionary_weak =
      host_zoom_dictionaries.FindDict(partition_key_);
  if (!host_zoom_dictionary_weak) {
    base::Value::Dict dict;
    host_zoom_dictionaries.Set(partition_key_, std::move(dict));
    host_zoom_dictionary_weak = host_zoom_dictionaries.FindDict(partition_key_);
  }

  if (modification_is_removal) {
    host_zoom_dictionary_weak->Remove(change.host);
  } else {
    base::Value::Dict dict;
    dict.Set(kZoomLevelKey, level);
    dict.Set(kLastModifiedPath,
             base::NumberToString(change.last_modified.ToInternalValue()));
    host_zoom_dictionary_weak->Set(change.host, std::move(dict));
  }
}

// TODO(wjmaclean): Remove the dictionary_path once the migration code is
// removed. crbug.com/420643
void ChromeZoomLevelPrefs::ExtractPerHostZoomLevels(
    const base::Value::Dict& host_zoom_dictionary,
    bool sanitize_partition_host_zoom_levels) {
  std::vector<std::string> keys_to_remove;
  base::Value::Dict host_zoom_dictionary_copy = host_zoom_dictionary.Clone();
  for (auto [host, value] : host_zoom_dictionary_copy) {
    std::optional<double> maybe_zoom;
    base::Time last_modified;

    if (value.is_dict()) {
      base::Value::Dict& dict = value.GetDict();
      if (dict.empty())
        continue;

      maybe_zoom = dict.FindDouble(kZoomLevelKey);
      last_modified = GetTimeStamp(dict);
    } else {
      // Old zoom level that is stored directly as a double.
      maybe_zoom = value.GetIfDouble();
    }

    // Filter out A) the empty host, B) zoom levels equal to the default; and
    // remember them, so that we can later erase them from Prefs.
    // Values of type A and B could have been stored due to crbug.com/364399.
    // Values of type B could further have been stored before the default zoom
    // level was set to its current value. In either case, SetZoomLevelForHost
    // will ignore type B values, thus, to have consistency with HostZoomMap's
    // internal state, these values must also be removed from Prefs.
    if (host.empty() || !maybe_zoom.has_value() ||
        blink::ZoomValuesEqual(maybe_zoom.value_or(0),
                               host_zoom_map_->GetDefaultZoomLevel())) {
      keys_to_remove.push_back(host);
      continue;
    }

    host_zoom_map_->InitializeZoomLevelForHost(host, maybe_zoom.value(),
                                               last_modified);
  }

  // We don't bother sanitizing non-partition dictionaries as they will be
  // discarded in the migration process. Note: since the structure of partition
  // per-host zoom level dictionaries is different from the legacy profile
  // per-host zoom level dictionaries, the following code will fail if run
  // on the legacy dictionaries.
  if (!sanitize_partition_host_zoom_levels)
    return;

  // Sanitize prefs to remove entries that match the default zoom level and/or
  // have an empty host.
  {
    ScopedDictPrefUpdate update(pref_service_,
                                prefs::kPartitionPerHostZoomLevels);
    base::Value::Dict& host_zoom_dictionaries = update.Get();
    base::Value::Dict* partition_dictionary =
        host_zoom_dictionaries.FindDict(partition_key_);
    for (const std::string& s : keys_to_remove)
      partition_dictionary->Remove(s);
  }
}

void ChromeZoomLevelPrefs::InitHostZoomMap(
    content::HostZoomMap* host_zoom_map) {
  // This init function must be called only once.
  DCHECK(!host_zoom_map_);
  DCHECK(host_zoom_map);
  host_zoom_map_ = host_zoom_map;

  // Initialize the default zoom level.
  host_zoom_map_->SetDefaultZoomLevel(GetDefaultZoomLevelPref());

  // Initialize the HostZoomMap with per-host zoom levels from the persisted
  // zoom-level preference values.
  const base::Value::Dict& host_zoom_dictionaries =
      pref_service_->GetDict(prefs::kPartitionPerHostZoomLevels);
  const base::Value::Dict* host_zoom_dictionary =
      host_zoom_dictionaries.FindDict(partition_key_);
  if (host_zoom_dictionary != nullptr) {
    // Since we're calling this before setting up zoom_subscription_ below we
    // don't need to worry that host_zoom_dictionary is indirectly affected by
    // calls to HostZoomMap::SetZoomLevelForHost().
    ExtractPerHostZoomLevels(*host_zoom_dictionary,
                             true /* sanitize_partition_host_zoom_levels */);
  }
  zoom_subscription_ =
      host_zoom_map_->AddZoomLevelChangedCallback(base::BindRepeating(
          &ChromeZoomLevelPrefs::OnZoomLevelChanged, base::Unretained(this)));

// On Android, the default zoom level is controlled by the Java-side UI and not
// the settings flow as it is with other platforms. For Android, we will pass an
// extra callback to the HostZoomMapImpl for setting the default zoom Pref when
// Java sends an update through the JNI. The HostZoomMapImpl cannot directly
// depend on this file, which will save the Pref to the storage partition.
#if BUILDFLAG(IS_ANDROID)
  host_zoom_map_->SetDefaultZoomLevelPrefCallback(base::BindRepeating(
      &ChromeZoomLevelPrefs::SetDefaultZoomLevelPref, base::Unretained(this)));
#endif
}
