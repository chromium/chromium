// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_type.h"

namespace user_manager {

const char* UserTypeToString(UserType user_type) {
  // Used in crash key for UserManagerImpl::UserLoggedIn.
  switch (user_type) {
    case UserType::kRegular:
      return "regular";
    case UserType::kChild:
      return "child";
    case UserType::kGuest:
      return "guest";
    case UserType::kPublicAccount:
      return "managed-guest-session";
    case UserType::kKioskApp:
      return "chrome-app-kiosk";
    case UserType::kWebKioskApp:
      return "web-kiosk";
    case UserType::kKioskIWA:
      return "iwa-kiosk";
  }
}

}  // namespace user_manager
