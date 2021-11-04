// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
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
base::Time TimeFromString(const std::string* serialized_time) {
  int64_t timestamp = 0;
  if (!serialized_time || !base::StringToInt64(*serialized_time, &timestamp))
    return {};

  return base::Time::FromInternalValue(timestamp);
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
  if (blink::PageZoomValuesEqual(level, host_zoom_map_->GetDefaultZoomLevel()))
    return;

  DictionaryPrefUpdate update(pref_service_, prefs::kPartitionDefaultZoomLevel);
  update->SetDoubleKey(partition_key_, level);
  // For unregistered paths, OnDefaultZoomLevelChanged won't be called, so
  // set this manually.
  host_zoom_map_->SetDefaultZoomLevel(level);
  default_zoom_changed_callbacks_.Notify();
  if (zoom_event_manager_)
    zoom_event_manager_->OnDefaultZoomLevelChanged();
}

double ChromeZoomLevelPrefs::GetDefaultZoomLevelPref() const {
  const base::Value* defaults =
      pref_service_->GetDictionary(prefs::kPartitionDefaultZoomLevel);
  return defaults->FindDoubleKey(partition_key_).value_or(0.0);
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

  const double default_level = host_zoom_map_->GetDefaultZoomLevel();
  double level = change.zoom_level;

  DictionaryPrefUpdate update(pref_service_,
                              prefs::kPartitionPerHostZoomLevels);
  base::Value* pref_dict = update.Get();
  DCHECK(pref_dict);

  base::Value* host_dict = GetOrCreateHostDictionary(pref_dict);

  if (blink::PageZoomValuesEqual(level, default_level)) {
    host_dict->RemoveKey(change.host);
  } else {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetDoubleKey(kZoomLevelKey, level);
    dict.SetStringKey(
        kLastModifiedPath,
        base::NumberToString(change.last_modified.ToInternalValue()));
    host_dict->SetKey(change.host, std::move(dict));
  }
}

void ChromeZoomLevelPrefs::ExtractPerHostZoomLevels(
    const base::Value* host_dict) {
  std::vector<std::string> keys_to_remove;
  const double default_zoom_level = host_zoom_map_->GetDefaultZoomLevel();

  for (auto it : host_dict->DictItems()) {
    const std::string& host = it.first;
    if (host.empty()) {
      keys_to_remove.push_back(host);
      continue;
    }

    double zoom_level =
        it.second.FindDoubleKey(kZoomLevelKey).value_or(default_zoom_level);
    base::Time last_modified =
        TimeFromString(it.second.FindStringKey(kLastModifiedPath));

    // Filter out zoom levels equal to the default and remember them, so that we
    // can later erase them from Prefs. These could have been stored before the
    // default zoom level was set to its current value. SetZoomLevelForHost will
    // ignore them, but we should clean them up here too to keep the pref store
    // consistent.
    if (blink::PageZoomValuesEqual(zoom_level, default_zoom_level)) {
      keys_to_remove.push_back(host);
      continue;
    }

    host_zoom_map_->InitializeZoomLevelForHost(host, zoom_level, last_modified);
  }

  // Sanitize prefs to remove entries that match the default zoom level and/or
  // have an empty host.
  {
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kPartitionPerHostZoomLevels);
    base::Value* pref_dictionaries = update.Get();
    base::Value* partition_dictionary =
        pref_dictionaries->FindDictKey(partition_key_);
    for (const std::string& host : keys_to_remove)
      partition_dictionary->RemoveKey(host);
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
  const base::Value* pref_dict =
      pref_service_->GetDictionary(prefs::kPartitionPerHostZoomLevels);
  const base::Value* host_dict = pref_dict->FindDictKey(partition_key_);
  if (host_dict) {
    // Since we're calling this before setting up zoom_subscription_ below we
    // don't need to worry that host_zoom_dictionary is indirectly affected by
    // calls to HostZoomMap::SetZoomLevelForHost().
    ExtractPerHostZoomLevels(host_dict);
  }

  zoom_subscription_ =
      host_zoom_map_->AddZoomLevelChangedCallback(base::BindRepeating(
          &ChromeZoomLevelPrefs::OnZoomLevelChanged, base::Unretained(this)));
}

base::Value* ChromeZoomLevelPrefs::GetOrCreateHostDictionary(
    base::Value* pref_dict) {
  if (!pref_dict->FindKey(partition_key_)) {
    pref_dict->SetKey(partition_key_,
                      base::Value(base::Value::Type::DICTIONARY));
  }
  return pref_dict->FindKey(partition_key_);
}
