// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_TAB_READINESS_CHECKER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_TAB_READINESS_CHECKER_H_

#include "content/public/browser/web_contents.h"

// An interface for objects that checks if the tab (`web_contents`) is ready to
// perform some action.
// This is created to ease testing.
class TabReadinessChecker {
 public:
  TabReadinessChecker() = default;
  virtual ~TabReadinessChecker() = default;
  virtual bool IsReadyForActionChipsRetrieval(
      const content::WebContents* web_contents) const = 0;
};

// An implementation of `TabReadinessCheckerImpl`.
class TabReadinessCheckerImpl : public TabReadinessChecker {
 public:
  bool IsReadyForActionChipsRetrieval(
      const content::WebContents* web_contents) const override;
  static const TabReadinessChecker* Get();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_TAB_READINESS_CHECKER_H_
