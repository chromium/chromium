// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using StartupBrowserCreatorTest = InProcessBrowserTest;

// Chrome OS doesn't support multiprofile.
// And BrowserWindow::IsActive() always returns false in tests on MAC.
// And this test is useless without that functionality.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, LastUsedProfileActivated) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 4 profiles, they will be scheduled for destruction when the last
  // browser window they are associated to will be closed.
  Profile& profile_1 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->user_data_dir().Append(
                           FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile_2 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->user_data_dir().Append(
                           FILE_PATH_LITERAL("New Profile 2")));
  Profile& profile_3 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->user_data_dir().Append(
                           FILE_PATH_LITERAL("New Profile 3")));
  Profile& profile_4 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->user_data_dir().Append(
                           FILE_PATH_LITERAL("New Profile 4")));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  SessionStartupPref::SetStartupPref(&profile_1, pref_urls);
  SessionStartupPref::SetStartupPref(&profile_2, pref_urls);
  SessionStartupPref::SetStartupPref(&profile_3, pref_urls);
  SessionStartupPref::SetStartupPref(&profile_4, pref_urls);

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile_1);
  last_opened_profiles.push_back(&profile_2);
  last_opened_profiles.push_back(&profile_3);
  last_opened_profiles.push_back(&profile_4);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {&profile_2, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  while (!browser_creator.ActivatedProfile())
    base::RunLoop().RunUntilIdle();

  Browser* new_browser = nullptr;

  // The last used profile (the profile_2 in this case) must be active.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_2));
  new_browser = chrome::FindBrowserWithProfile(&profile_2);
  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(new_browser->window()->IsActive());

  // All other profiles browser should not be active.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_1));
  new_browser = chrome::FindBrowserWithProfile(&profile_1);
  ASSERT_TRUE(new_browser);
  EXPECT_FALSE(new_browser->window()->IsActive());

  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_3));
  new_browser = chrome::FindBrowserWithProfile(&profile_3);
  ASSERT_TRUE(new_browser);
  EXPECT_FALSE(new_browser->window()->IsActive());

  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_4));
  new_browser = chrome::FindBrowserWithProfile(&profile_4);
  ASSERT_TRUE(new_browser);
  EXPECT_FALSE(new_browser->window()->IsActive());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MAC)

#if defined(USE_AURA)
class StartupPagePrefSetterMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  explicit StartupPagePrefSetterMainExtraParts(const std::vector<GURL>& urls)
      : urls_(urls) {}
  StartupPagePrefSetterMainExtraParts(
      const StartupPagePrefSetterMainExtraParts&) = delete;
  StartupPagePrefSetterMainExtraParts& operator=(
      const StartupPagePrefSetterMainExtraParts&) = delete;

  // ChromeBrowserMainExtraParts:
  void PreBrowserStart() override {
    Profile* profile =
        g_browser_process->profile_manager()->GetLastUsedProfile();

    SessionStartupPref pref_urls(SessionStartupPref::URLS);
    pref_urls.urls = std::move(urls_);
    SessionStartupPref::SetStartupPref(profile, pref_urls);
  }

 private:
  std::vector<GURL> urls_;
};

class StartupPageTest : public InProcessBrowserTest {
 public:
  StartupPageTest() {
    // Don't open about:blank since we want to test startup urls.
    set_open_about_blank_on_browser_launch(false);
  }
  StartupPageTest(const StartupPageTest&) = delete;
  StartupPageTest& operator=(const StartupPageTest&) = delete;
  ~StartupPageTest() override = default;

  // InProcessBrowserTest:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    const std::vector<GURL> urls = {ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("focus")),
        base::FilePath(FILE_PATH_LITERAL("page_with_focus.html")))};

    ChromeBrowserMainParts* chrome_browser_main_parts =
        static_cast<ChromeBrowserMainParts*>(browser_main_parts);
    chrome_browser_main_parts->AddParts(
        std::make_unique<StartupPagePrefSetterMainExtraParts>(urls));
  }
};

IN_PROC_BROWSER_TEST_F(StartupPageTest, StartupPageFocus) {
  // Browser window should be active.
  EXPECT_TRUE(browser()->window()->IsActive());

  // Focus should land in the content area.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(contents->GetContentNativeView()->HasFocus());
}
#endif  // defined(USE_AURA)
