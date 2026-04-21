// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/launch_or_reparent_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os, LaunchOrReparentResult result) {
  switch (result) {
    case LaunchOrReparentResult::kReparented:
      return os << "Reparented";
    case LaunchOrReparentResult::kLaunched:
      return os << "Launched";
    case LaunchOrReparentResult::kWebContentsGone:
      return os << "WebContentsGone";
    case LaunchOrReparentResult::kAppNotInstalledAsDedicatedWindow:
      return os << "AppNotInstalledAsDedicatedWindow";
    case LaunchOrReparentResult::kShutdown:
      return os << "Shutdown";
  }
}

}  // namespace web_app
