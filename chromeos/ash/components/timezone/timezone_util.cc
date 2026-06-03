// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/timezone/timezone_util.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace ash::system {
namespace {

bool CanSetSystemTimezoneFromManagedGuestSession(
    const PrefService& local_state) {
  const int automatic_detection_policy = local_state.GetInteger(
      ash::prefs::kSystemTimezoneAutomaticDetectionPolicy);

  return (automatic_detection_policy ==
          enterprise_management::SystemTimezoneProto::DISABLED) ||
         (automatic_detection_policy ==
          enterprise_management::SystemTimezoneProto::USERS_DECIDE);
}

}  // namespace

// TODO(b/353580799): Add unit tests for this function.
bool CanSetSystemTimezone(const PrefService& local_state,
                          const user_manager::User* user) {
  if (!user->is_logged_in()) {
    return false;
  }

  switch (user->GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kKioskChromeApp:
    case user_manager::UserType::kKioskWebApp:
    case user_manager::UserType::kKioskIWA:
    case user_manager::UserType::kKioskArcvmApp:
    case user_manager::UserType::kChild:
      return true;

    case user_manager::UserType::kGuest:
      return false;

    case user_manager::UserType::kPublicAccount:
      return CanSetSystemTimezoneFromManagedGuestSession(local_state);

      // No default case means the compiler makes sure we handle new types.
  }
  NOTREACHED();
}

bool SetSystemTimezone(const PrefService& local_state,
                       const user_manager::User* user,
                       const std::string& timezone) {
  DCHECK(user);
  if (!CanSetSystemTimezone(local_state, user)) {
    return false;
  }
  TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16(timezone));
  return true;
}

void SetSystemAndSigninScreenTimezone(PrefService& local_state,
                                      const std::string& timezone) {
  if (timezone.empty()) {
    return;
  }

  local_state.SetString(ash::prefs::kSigninScreenTimezone, timezone);

  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);
  if (current_timezone_id != timezone) {
    ash::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
        base::UTF8ToUTF16(timezone));
  }
}

bool IsTimezonePrefsManaged(const PrefService& local_state,
                            const std::string& pref_name) {
  DCHECK(pref_name == kSystemTimezone ||
         pref_name == ash::prefs::kUserTimezone ||
         pref_name == ash::prefs::kResolveTimezoneByGeolocationMethod);

  std::string policy_timezone;
  if (CrosSettings::Get()->GetString(kSystemTimezonePolicy, &policy_timezone) &&
      !policy_timezone.empty()) {
    return true;
  }

  // System timezone preference is managed only if kSystemTimezonePolicy
  // present, which we checked above.
  //
  // kSystemTimezoneAutomaticDetectionPolicy (see below) controls only user
  // time zone preference, and user time zone resolve preference.
  if (pref_name == kSystemTimezone) {
    return false;
  }

  if (!local_state.IsManagedPreference(
          ash::prefs::kSystemTimezoneAutomaticDetectionPolicy)) {
    return false;
  }

  int resolve_policy_value = local_state.GetInteger(
      ash::prefs::kSystemTimezoneAutomaticDetectionPolicy);

  switch (resolve_policy_value) {
    case enterprise_management::SystemTimezoneProto::USERS_DECIDE:
      return false;
    case enterprise_management::SystemTimezoneProto::DISABLED:
      // This only disables resolving.
      return pref_name == ash::prefs::kResolveTimezoneByGeolocationMethod;
    case enterprise_management::SystemTimezoneProto::IP_ONLY:
    case enterprise_management::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS:
    case enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO:
      return true;
  }
  // Default for unknown policy value.
  NOTREACHED() << "Unrecognized policy value: " << resolve_policy_value;
}

}  // namespace ash::system
