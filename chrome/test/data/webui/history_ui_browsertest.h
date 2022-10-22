// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/web_ui_browser_test.h"

namespace history {
class HistoryService;
}

class HistoryUIBrowserTest : public WebUIBrowserTest {
 public:
  HistoryUIBrowserTest();

  HistoryUIBrowserTest(const HistoryUIBrowserTest&) = delete;
  HistoryUIBrowserTest& operator=(const HistoryUIBrowserTest&) = delete;

  ~HistoryUIBrowserTest() override;

  void SetUpOnMainThread() override;

 protected:
  // Sets the pref to allow or prohibit deleting history entries.
  void SetDeleteAllowed(bool allowed);

 private:
  // The HistoryService is owned by the profile.
  raw_ptr<history::HistoryService, DanglingUntriaged> history_;
};

#endif  // CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
