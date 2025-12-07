// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_TYPES_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_TYPES_H_

#include "ui/base/window_open_disposition.h"

namespace user_education {

// Specifies how a page should be opened.
enum class PageOpenMode {
  kOverwriteActiveTab,
  kNewForegroundTab,
  kSingletonTab
};

// Returns the WindowOpenDisposition corresponding to the given PageOpenMode.
// This should be used to determine how a page should be opened.
WindowOpenDisposition GetWindowOpenDisposition(
      user_education::PageOpenMode page_open_mode);

}  // namespace user_education

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_TYPES_H_

