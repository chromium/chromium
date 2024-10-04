// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_TYPE_H_
#define COMPONENTS_USER_MANAGER_USER_TYPE_H_

#include <ostream>
#include <string_view>

#include "base/component_export.h"

namespace user_manager {

// The user type. Used in a histogram; do not modify existing types.
// When adding a new one, also update histograms/enums.xml. Note that types are
// not sorted by number but grouped by means instead.
enum class UserType {
  // Regular user, has a user name, password and Gaia account. (@gmail.com,
  // managed commercial and EDU accounts). These users are usually connected to
  // Google services (sync, etc.). Could be ephemeral (data wiped on logout)
  // depending on the device policy.
  kRegular = 0,
  // Child user, with supervised options. Same as Regular but has a user policy
  // which is controlled by parents.
  kChild = 6,

  // Guest user, logs in without authentication. No Gaia account. Always
  // ephemeral.
  kGuest = 1,

  // kRetailMode = 2, // deprecated

  // Public account user, logs in without authentication. Available only if
  // enabled through device policy. No Gaia account. Always ephemeral.
  kPublicAccount = 3,

  // kSupervisedDeprecated = 4,

  // Kiosk users used to launch application in a single app mode. Logs in
  // without authentications. No Gaia user account. Uses device robot account.
  // Ephemeral for demo mode only.
  // Kiosk type for Chrome apps.
  kKioskApp = 5,
  // kArcKioskApp = 7, deprecated
  // Kiosk type for Web apps (aka PWA - Progressive Web Apps).
  kWebKioskApp = 9,

  // Kiosk type for Isolated Web Apps (IWA)
  kKioskIWA = 10,

  // Active Directory user. Authenticates against Active Directory server. No
  // Gaia account. Could be ephemeral depending on the device policy.
  // kActiveDirectory = 8,    // deprecated

  // Alias for histogram.
  kMaxValue = kKioskIWA,
};

// Stringifies UserType. Returns a C-style (i.e. \0-terminated) string literal.
// The returned value is for logging or also to be used for crash key in
// UserManager.
COMPONENT_EXPORT(USER_MANAGER_COMMON)
const char* UserTypeToString(UserType user_type);

// Operator overloading for logging.
inline std::ostream& operator<<(std::ostream& os, UserType user_type) {
  return os << UserTypeToString(user_type);
}

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_TYPE_H_
