// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_TYPE_H_
#define COMPONENTS_USER_MANAGER_USER_TYPE_H_

namespace user_manager {

// The user type. Used in a histogram; do not modify existing types.
// When adding a new one, also update histograms/enums.xml. Note that types are
// not sorted by number but grouped by means instead.
typedef enum {
  // Regular user, has a user name, password and Gaia account. (@gmail.com,
  // managed commercial and EDU accounts). These users are usually connected to
  // Google services (sync, etc.). Could be ephemeral (data wiped on logout)
  // depending on the device policy.
  USER_TYPE_REGULAR = 0,
  // Child user, with supervised options. Same as Regular but has a user policy
  // which is controlled by parents.
  USER_TYPE_CHILD = 6,

  // Guest user, logs in without authentication. No Gaia account. Always
  // ephemeral.
  USER_TYPE_GUEST = 1,

  // USER_TYPE_RETAIL_MODE = 2, // deprecated

  // Public account user, logs in without authentication. Available only if
  // enabled through device policy. No Gaia account. Always ephemeral.
  USER_TYPE_PUBLIC_ACCOUNT = 3,

  // USER_TYPE_SUPERVISED_DEPRECATED = 4,

  // Kiosk users used to launch application in a single app mode. Logs in
  // without authentications. No Gaia user account. Uses device robot account.
  // Ephemeral for demo mode only.
  // Kiosk type for Chrome apps.
  USER_TYPE_KIOSK_APP = 5,
  // Kiosk type for Android apps.
  USER_TYPE_ARC_KIOSK_APP = 7,
  // Kiosk type for Web apps (aka PWA - Progressive Web Apps).
  USER_TYPE_WEB_KIOSK_APP = 9,

  // Active Directory user. Authenticates against Active Directory server. No
  // Gaia account. Could be ephemeral depending on the device policy.
  // USER_TYPE_ACTIVE_DIRECTORY = 8,    // deprecated

  // Maximum histogram value.
  NUM_USER_TYPES = 10
} UserType;

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_TYPE_H_
