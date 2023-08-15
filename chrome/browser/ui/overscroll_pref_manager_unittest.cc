// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/overscroll_pref_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::WebContentsTester;

class OverscrollPrefManagerTest : public BrowserWithTestWindowTest {
 protected:
  void InsertTab(Browser* browser) {
    std::unique_ptr<WebContents> contents =
        WebContentsTester::CreateTestWebContents(profile(), nullptr);
    browser->tab_strip_model()->AppendWebContents(std::move(contents),
                                                  /*foreground=*/true);
  }
};

// Tests that changing the pref immediately enables or disables overscroll
TEST_F(OverscrollPrefManagerTest, PrefChange) {
  InsertTab(browser());

  PrefService* local_state = g_browser_process->local_state();
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsTester* test_web_contents = WebContentsTester::For(web_contents);

  ASSERT_TRUE(
      local_state->GetBoolean(prefs::kOverscrollHistoryNavigationEnabled));
  ASSERT_TRUE(browser()->CanOverscrollContent());

  local_state->SetBoolean(prefs::kOverscrollHistoryNavigationEnabled, false);
  EXPECT_FALSE(browser()->CanOverscrollContent());
  EXPECT_FALSE(test_web_contents->GetOverscrollNavigationEnabled());

  local_state->SetBoolean(prefs::kOverscrollHistoryNavigationEnabled, true);
  EXPECT_TRUE(browser()->CanOverscrollContent());
  EXPECT_TRUE(test_web_contents->GetOverscrollNavigationEnabled());
}
