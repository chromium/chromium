// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_type.h"

namespace user_manager {

const char* UserTypeToString(UserType user_type) {
  // Used in crash key for UserManagerBase::UserLoggedIn.
  switch (user_type) {
    case USER_TYPE_REGULAR:
      return "regular";
    case USER_TYPE_CHILD:
      return "child";
    case USER_TYPE_GUEST:
      return "guest";
    case USER_TYPE_PUBLIC_ACCOUNT:
      return "managed-guest-session";
    case USER_TYPE_KIOSK_APP:
      return "chrome-app-kiosk";
    case USER_TYPE_ARC_KIOSK_APP:
      return "arc-kiosk";
    case USER_TYPE_WEB_KIOSK_APP:
      return "web-kiosk";
  }
}

}  // namespace user_manager
