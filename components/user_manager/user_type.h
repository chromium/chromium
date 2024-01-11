// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_TYPE_H_
#define COMPONENTS_USER_MANAGER_USER_TYPE_H_

#include <ostream>
#include <string_view>

#include "components/user_manager/user_manager_export.h"

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

  // USER_TYPE_RETAIL_MODE = 2, // deprecated

  // Public account user, logs in without authentication. Available only if
  // enabled through device policy. No Gaia account. Always ephemeral.
  kPublicAccount = 3,

  // USER_TYPE_SUPERVISED_DEPRECATED = 4,

  // Kiosk users used to launch application in a single app mode. Logs in
  // without authentications. No Gaia user account. Uses device robot account.
  // Ephemeral for demo mode only.
  // Kiosk type for Chrome apps.
  kKioskApp = 5,
  // Kiosk type for Android apps.
  kArcKioskApp = 7,
  // Kiosk type for Web apps (aka PWA - Progressive Web Apps).
  kWebKioskApp = 9,

  // Active Directory user. Authenticates against Active Directory server. No
  // Gaia account. Could be ephemeral depending on the device policy.
  // USER_TYPE_ACTIVE_DIRECTORY = 8,    // deprecated

  // Alias for histogram.
  kMaxValue = kWebKioskApp,

  // DEPRECATED: legacy name aliases for transition period.
  // TODO(b/278643115): Remove them.
  USER_TYPE_REGULAR = kRegular,
  USER_TYPE_CHILD = kChild,
  USER_TYPE_GUEST = kGuest,
  USER_TYPE_PUBLIC_ACCOUNT = kPublicAccount,
  USER_TYPE_KIOSK_APP = kKioskApp,
  USER_TYPE_ARC_KIOSK_APP = kArcKioskApp,
  USER_TYPE_WEB_KIOSK_APP = kWebKioskApp,
};

// DEPRECATED: legacy name aliases for transition period.
// TODO(b/278643115): Remove them.
inline constexpr UserType USER_TYPE_REGULAR = UserType::kRegular;
inline constexpr UserType USER_TYPE_CHILD = UserType::kChild;
inline constexpr UserType USER_TYPE_GUEST = UserType::kGuest;
inline constexpr UserType USER_TYPE_PUBLIC_ACCOUNT = UserType::kPublicAccount;
inline constexpr UserType USER_TYPE_KIOSK_APP = UserType::kKioskApp;
inline constexpr UserType USER_TYPE_ARC_KIOSK_APP = UserType::kArcKioskApp;
inline constexpr UserType USER_TYPE_WEB_KIOSK_APP = UserType::kWebKioskApp;

// Stringifies UserType. Returns a C-style (i.e. \0-terminated) string literal.
// The returned value is for logging or also to be used for crash key in
// UserManager.
USER_MANAGER_EXPORT const char* UserTypeToString(UserType user_type);

// Operator overloading for logging.
inline std::ostream& operator<<(std::ostream& os, UserType user_type) {
  return os << UserTypeToString(user_type);
}

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_TYPE_H_
