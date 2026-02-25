// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UTIL_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UTIL_H_

#include <string_view>

namespace borealis {

// Gets the current milestone from the version information, to be used
// when requesting the MOTD message for the Borealis MOTD dialog
int GetMilestone();

// Possible user actions in the MOTD dialog
enum class UserMotdAction {
  kDismiss,
  kUninstall,
};

// Translates a user action to string to be sent to the page handler
// as JSON data
const char* GetUserActionString(UserMotdAction action);

// Translates an user action string to UserMotdAction to get data from
// MOTD page handler JSON data
UserMotdAction GetUserActionFromString(std::string_view action);

}  // namespace borealis

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UTIL_H_
