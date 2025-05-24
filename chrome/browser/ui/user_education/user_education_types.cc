// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/user_education_types.h"

namespace user_education {
WindowOpenDisposition GetWindowOpenDisposition(
    PageOpenMode page_open_mode) {
  switch (page_open_mode) {
    case PageOpenMode::kOverwriteActiveTab:
      return WindowOpenDisposition::CURRENT_TAB;
    case PageOpenMode::kNewForegroundTab:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case PageOpenMode::kSingletonTab:
      return WindowOpenDisposition::SINGLETON_TAB;
  }
}
}  // namespace user_education
