// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ENUMS_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ENUMS_H_

namespace page_actions {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PageActionCTREvent {
  kShown = 0,
  kClicked,
  kMaxValue = kClicked,
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ENUMS_H_
