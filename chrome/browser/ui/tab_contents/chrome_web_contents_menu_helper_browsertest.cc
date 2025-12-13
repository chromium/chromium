// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_menu_helper.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace {

using ChromeWebContentsMenuHelperBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeWebContentsMenuHelperBrowserTest,
                       AllowContextMenuAccessThroughPreferences) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kDefaultSearchProviderContextMenuAccessAllowed, true);

  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://foo/1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::ContextMenuParams enriched_params =
      AddContextMenuParamsPropertiesFromPreferences(
          browser()->tab_strip_model()->GetWebContentsAt(0),
          content::ContextMenuParams());
  EXPECT_EQ(1U, enriched_params.properties.count(
                    prefs::kDefaultSearchProviderContextMenuAccessAllowed));
}

IN_PROC_BROWSER_TEST_F(ChromeWebContentsMenuHelperBrowserTest,
                       DisallowContextMenuAccessThroughPreferences) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kDefaultSearchProviderContextMenuAccessAllowed, false);

  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://foo/1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::ContextMenuParams enriched_params =
      AddContextMenuParamsPropertiesFromPreferences(
          browser()->tab_strip_model()->GetWebContentsAt(0),
          content::ContextMenuParams());
  EXPECT_EQ(0U, enriched_params.properties.size());
}

}  // namespace
