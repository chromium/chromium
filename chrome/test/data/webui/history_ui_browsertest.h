// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_

#include "base/macros.h"
#include "chrome/test/base/web_ui_browser_test.h"

namespace history {
class HistoryService;
}

class HistoryUIBrowserTest : public WebUIBrowserTest {
 public:
  HistoryUIBrowserTest();
  ~HistoryUIBrowserTest() override;

  void SetUpOnMainThread() override;

 protected:
  // Sets the pref to allow or prohibit deleting history entries.
  void SetDeleteAllowed(bool allowed);

 private:
  // The HistoryService is owned by the profile.
  history::HistoryService* history_;

  DISALLOW_COPY_AND_ASSIGN(HistoryUIBrowserTest);
};

#endif  // CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
