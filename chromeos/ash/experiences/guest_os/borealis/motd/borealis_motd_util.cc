// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_util.h"

#include <utility>

#include "base/logging.h"
#include "base/version.h"
#include "components/version_info/version_info.h"

namespace borealis {

constexpr char kUserActionDismiss[] = "dismiss";
constexpr char kUserActionUninstall[] = "uninstall";

int GetMilestone() {
  return version_info::GetVersion().components()[0];
}

const char* GetUserActionString(UserMotdAction action) {
  switch (action) {
    case UserMotdAction::kDismiss:
      return kUserActionDismiss;
    case UserMotdAction::kUninstall:
      return kUserActionUninstall;
  }
  LOG(FATAL) << "Unknown action: " << std::to_underlying(action);
}

UserMotdAction GetUserActionFromString(std::string_view action) {
  if (action == kUserActionDismiss) {
    return UserMotdAction::kDismiss;
  }
  if (action == kUserActionUninstall) {
    return UserMotdAction::kUninstall;
  }

  LOG(FATAL) << "Unknown action: " << action;
}

}  // namespace borealis
