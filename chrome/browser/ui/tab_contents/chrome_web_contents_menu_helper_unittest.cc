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
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/context_menu_params.h"

using ::testing::Eq;

namespace {
class ChromeWebContentsMenuHelperUnitTest : public BrowserWithTestWindowTest {
 protected:
  void TearDown() override {
    pref_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs(
        new sync_preferences::TestingPrefServiceSyncable);
    RegisterUserProfilePrefs(prefs->registry());
    pref_service_ = prefs.get();

    auto* profile = profile_manager()->CreateTestingProfile(
        profile_name, std::move(prefs), std::u16string(), 0,
        TestingProfile::TestingFactories());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    OnUserProfileCreated(profile_name, profile);
#endif
    return profile;
  }

  sync_preferences::PrefServiceSyncable* pref_service() {
    return pref_service_;
  }

 private:
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
};
}  // namespace

TEST_F(ChromeWebContentsMenuHelperUnitTest,
       AllowContextMenuAccessThroughPreferences) {
  pref_service()->SetBoolean(
      prefs::kDefaultSearchProviderContextMenuAccessAllowed, true);

  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  AddTab(browser(), GURL("http://foo/1"));

  content::ContextMenuParams enriched_params =
      AddContextMenuParamsPropertiesFromPreferences(
          browser()->tab_strip_model()->GetWebContentsAt(0),
          content::ContextMenuParams());
  EXPECT_EQ(1U, enriched_params.properties.count(
                    prefs::kDefaultSearchProviderContextMenuAccessAllowed));
}

TEST_F(ChromeWebContentsMenuHelperUnitTest,
       DisallowContextMenuAccessThroughPreferences) {
  pref_service()->SetBoolean(
      prefs::kDefaultSearchProviderContextMenuAccessAllowed, false);

  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  AddTab(browser(), GURL("http://foo/1"));

  content::ContextMenuParams enriched_params =
      AddContextMenuParamsPropertiesFromPreferences(
          browser()->tab_strip_model()->GetWebContentsAt(0),
          content::ContextMenuParams());
  EXPECT_EQ(0U, enriched_params.properties.size());
}
