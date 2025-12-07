// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_role_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash::boca_util {

namespace {
inline constexpr std::string_view kDisabled = "disabled";
inline constexpr std::string_view kStudent = "student";
inline constexpr std::string_view kRemoteAdmin = "remote_admin_was_present";
}  // namespace

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      ash::prefs::kClassManagementToolsAvailabilitySetting, kDisabled);
  // Fixes a dangling pointer crash associated with this pref not being
  // registered. Need to revisit if this is the best place for this pref to be
  // set.
  registry->RegisterBooleanPref(kRemoteAdmin, false);
  registry->RegisterBooleanPref(
      ash::prefs::kClassManagementToolsCaptionEnablementSetting, false);
  registry->RegisterBooleanPref(
      ash::prefs::kClassManagementToolsCaptionEligibilitySetting, true);
  registry->RegisterBooleanPref(
      ash::prefs::kClassManagementToolsClassroomEligibilitySetting, true);
  registry->RegisterBooleanPref(
      ash::prefs::kClassManagementToolsViewScreenEligibilitySetting, true);
  registry->RegisterBooleanPref(
      ash::prefs::kClassManagementToolsNetworkRestrictionSetting, true);
  registry->RegisterDictionaryPref(
      ash::prefs::kClassManagementToolsNavRuleSetting);

  registry->RegisterIntegerPref(
      ash::prefs::kClassManagementToolsOOBEAccessCountSetting, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(
      ash::prefs::kClassManagementToolsKioskReceiverCodes,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

bool IsEnabled(const user_manager::User* user) {
  if (!ash::features::IsBocaUberEnabled()) {
    return false;
  }

  if (features::IsBocaEnabled()) {
    return true;
  }
  if (!user) {
    return false;
  }

  if (!user->IsAffiliated()) {
    return false;
  }

  auto* pref_service = user->GetProfilePrefs();

  if (!pref_service) {
    return false;
  }

  auto setting =
      pref_service->GetString(prefs::kClassManagementToolsAvailabilitySetting);
  // TODO(crbug.com/373484445): Currently check against empty in case value is
  // not set. Confirm if it can be removed.
  return !setting.empty() && setting != kDisabled;
}

bool IsConsumer(const user_manager::User* user) {
  if (features::IsBocaEnabled()) {
    return features::IsBocaConsumerEnabled();
  }

  if (!IsEnabled(user)) {
    return false;
  }
  auto* pref_service = user->GetProfilePrefs();

  CHECK(pref_service);
  return pref_service->GetString(
             prefs::kClassManagementToolsAvailabilitySetting) == kStudent;
}

bool IsProducer(const user_manager::User* user) {
  return IsEnabled(user) && !IsConsumer(user);
}
}  // namespace ash::boca_util
