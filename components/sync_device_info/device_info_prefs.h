// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PREFS_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PREFS_H_

#include <string>
#include <vector>

#include "base/macros.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class Clock;
}  // namespace base

namespace syncer {

// Use this for determining if a cache guid was recently used by this device.
class DeviceInfoPrefs {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static void MigrateRecentLocalCacheGuidsPref(PrefService* pref_service);

  // |pref_service| and |clock| must outlive this class and be non null.
  DeviceInfoPrefs(PrefService* pref_service, const base::Clock* clock);
  ~DeviceInfoPrefs();

  // Returns if the given |cache_guid| is present in the saved pref. This is
  // most reliable when dealing with recent devices only, due to garbage
  // collection of local GUIDs, as per kMaxDaysLocalCacheGuidsStored.
  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const;

  // Adds the given |cache_guid| to the internal list stored in prefs and
  // exposed via IsRecentLocalCacheGuid(). If the |cache_guid| already exists,
  // this will reset the expiry date for that entry.
  void AddLocalCacheGuid(const std::string& cache_guid);

  // Garbage-collects local cache GUIDs if too old.
  void GarbageCollectExpiredCacheGuids();

 private:
  PrefService* const pref_service_;
  const base::Clock* const clock_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoPrefs);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PREFS_H_
