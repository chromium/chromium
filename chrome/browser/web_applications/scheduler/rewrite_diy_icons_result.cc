// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/rewrite_diy_icons_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os, RewriteIconResult result) {
  switch (result) {
    case RewriteIconResult::kUnexpectedAppStateChange:
      return os << "kUnexpectedAppStateChange";
    case RewriteIconResult::kUpdateSucceeded:
      return os << "kUpdateSucceeded";
    case RewriteIconResult::kShortcutInfoFetchFailed:
      return os << "kShortcutInfoFetchFailed";
    case RewriteIconResult::kUpdateShortcutFailed:
      return os << "kUpdateShortcutFailed";
  }
}

}  // namespace web_app
