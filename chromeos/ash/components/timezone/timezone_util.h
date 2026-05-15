// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_UTIL_H_

#include <string>

class PrefService;

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash::system {

// Returns true if the given user is allowed to set the system timezone - that
// is, the single timezone at TimezoneSettings::GetInstance()->GetTimezone(),
// which is also stored in a file at /var/lib/timezone/localtime.
bool CanSetSystemTimezone(const PrefService& local_state,
                          const user_manager::User* user);

// Set system timezone to the given |timezone_id|, as long as the given |user|
// is allowed to set it (so not a guest or public account).
// Updates only the global system timezone - not specific to the user - and
// doesn't care if perUserTimezone is enabled.
// Returns |true| if the system timezone is set, false if the given user cannot.
bool SetSystemTimezone(const PrefService& local_state,
                       const user_manager::User* user,
                       const std::string& timezone);

// Updates Local State preference ash::prefs::kSigninScreenTimezone AND
// also immediately sets system timezone (ash::system::TimezoneSettings).
// This is called when there is no user session (i.e. OOBE and signin screen),
// or when device policies are updated.
void SetSystemAndSigninScreenTimezone(PrefService& local_state,
                                      const std::string& timezone);

}  // namespace ash::system

#endif  // CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_UTIL_H_
