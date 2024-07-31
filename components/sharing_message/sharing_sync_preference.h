// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_SYNC_PREFERENCE_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_SYNC_PREFERENCE_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {
class DeviceInfoSyncService;
class LocalDeviceInfoProvider;
}  // namespace syncer

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

enum class SharingDevicePlatform;

// SharingSyncPreference manages all preferences related to Sharing using Sync,
// such as storing list of user devices synced via Chrome and VapidKey used
// for authentication.
class SharingSyncPreference {
 public:
  // FCM registration status of current device. Not synced across devices.
  struct FCMRegistration {
    FCMRegistration(std::optional<std::string> authorized_entity,
                    base::Time timestamp);
    FCMRegistration(FCMRegistration&& other);
    FCMRegistration& operator=(FCMRegistration&& other);
    ~FCMRegistration();

    // Authorized entity registered with FCM.
    std::optional<std::string> authorized_entity;

    // Timestamp of latest registration.
    base::Time timestamp;
  };

  SharingSyncPreference(
      PrefService* prefs,
      syncer::DeviceInfoSyncService* device_info_sync_service);

  SharingSyncPreference(const SharingSyncPreference&) = delete;
  SharingSyncPreference& operator=(const SharingSyncPreference&) = delete;

  ~SharingSyncPreference();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns local SharingInfo to be uploaded to sync.
  static std::optional<syncer::DeviceInfo::SharingInfo>
  GetLocalSharingInfoForSync(PrefService* prefs);

  // Returns VAPID key from preferences if present, otherwise returns
  // std::nullopt.
  // For more information on vapid keys, please see
  // https://tools.ietf.org/html/draft-thomson-webpush-vapid-02
  std::optional<std::vector<uint8_t>> GetVapidKey() const;

  // Adds VAPID key to preferences for syncing across devices.
  void SetVapidKey(const std::vector<uint8_t>& vapid_key) const;

  // Observes for VAPID key changes. Replaces previously set observer.
  void SetVapidKeyChangeObserver(const base::RepeatingClosure& obs);

  // Clears previously set observer.
  void ClearVapidKeyChangeObserver();

  std::optional<FCMRegistration> GetFCMRegistration() const;

  void SetFCMRegistration(FCMRegistration registration);

  void ClearFCMRegistration();

  void SetLocalSharingInfo(syncer::DeviceInfo::SharingInfo sharing_info);

  void ClearLocalSharingInfo();

 private:
  friend class SharingSyncPreferenceTest;

  raw_ptr<PrefService> prefs_;
  raw_ptr<syncer::DeviceInfoSyncService> device_info_sync_service_;
  raw_ptr<syncer::LocalDeviceInfoProvider> local_device_info_provider_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_SYNC_PREFERENCE_H_
