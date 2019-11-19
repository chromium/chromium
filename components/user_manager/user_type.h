// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_TYPE_H_
#define COMPONENTS_USER_MANAGER_USER_TYPE_H_

namespace user_manager {

// The user type. Used in a histogram; do not modify existing types.
// When adding a new one, also update histograms/enums.xml.
typedef enum {
  // Regular user, has a user name and password.
  USER_TYPE_REGULAR = 0,
  // Guest user, logs in without authentication.
  USER_TYPE_GUEST = 1,
  /* USER_TYPE_RETAIL_MODE = 2, // deprecated */
  // Public account user, logs in without authentication. Available only if
  // enabled through policy.
  USER_TYPE_PUBLIC_ACCOUNT = 3,
  // Supervised user, logs in only with local authentication.
  USER_TYPE_SUPERVISED = 4,
  // Kiosk app robot, logs in without authentication.
  USER_TYPE_KIOSK_APP = 5,
  // Child user, with supervised options.
  USER_TYPE_CHILD = 6,
  // Android app in kiosk mode, logs in without authentication.
  USER_TYPE_ARC_KIOSK_APP = 7,
  // Active Directory user. Authenticates against Active Directory server.
  USER_TYPE_ACTIVE_DIRECTORY = 8,
  // Web app kiosk, logs in without authentication.
  USER_TYPE_WEB_KIOSK_APP = 9,
  // Maximum histogram value.
  NUM_USER_TYPES = 10
} UserType;

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_TYPE_H_
