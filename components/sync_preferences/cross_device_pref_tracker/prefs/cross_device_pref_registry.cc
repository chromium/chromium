// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_registry.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"

namespace cross_device {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kCrossDeviceCrossPlatformPromosIOS16thActiveDay,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCrossDeviceOmniboxIsInBottomPosition,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCrossDeviceSafetyCheckHomeModuleEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCrossDeviceTabResumptionHomeModuleEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCrossDeviceTipsHomeModuleEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace cross_device
