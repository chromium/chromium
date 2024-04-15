// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_deletion_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/browser_test_util.h"
#endif

namespace {

Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  return &profiles::testing::CreateProfileSync(profile_manager, new_path);
}

// An observer returns back to test code after brower window associated with
// the profile is activated.
class ExpectBrowserActivationForProfile : public BrowserListObserver {
 public:
  explicit ExpectBrowserActivationForProfile(Profile* profile)
      : profile_(profile) {
    BrowserList::AddObserver(this);
  }

  ~ExpectBrowserActivationForProfile() override {
    BrowserList::RemoveObserver(this);
  }

  void Wait() {
    loop_.Run();
  }

 protected:
  void OnBrowserSetLastActive(Browser* browser) override {
    if (browser->profile() == profile_)
      loop_.Quit();
  }

 private:
  raw_ptr<Profile> profile_;
  base::RunLoop loop_;
};

// An observer that returns back to test code after a new browser is added to
// the BrowserList.
class BrowserAddedObserver : public BrowserListObserver {
 public:
  BrowserAddedObserver() { BrowserList::AddObserver(this); }

  ~BrowserAddedObserver() override { BrowserList::RemoveObserver(this); }

  Browser* Wait() {
    run_loop_.Run();
    return browser_;
  }

 protected:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    browser_ = browser;
    run_loop_.Quit();
  }

 private:
  raw_ptr<Browser> browser_ = nullptr;
  base::RunLoop run_loop_;
};

}  // namespace

class ProfileHelperTest : public InProcessBrowserTest {
 public:
  ProfileHelperTest() = default;

 protected:
  void SetUp() override {
    // Shortcut deletion delays tests shutdown on Win-7 and results in time out.
    // See crbug.com/1073451.
#if BUILDFLAG(IS_WIN)
    AppShortcutManager::SuppressShortcutsForTesting();
#endif
    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(ProfileHelperTest, OpenNewWindowForProfile) {
  BrowserList* browser_list = BrowserList::GetInstance();

  Browser* original_browser = browser();
  Profile* original_profile = original_browser->profile();
  std::unique_ptr<ExpectBrowserActivationForProfile> activation_observer;

  // Sanity checks.
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_TRUE(base::Contains(*browser_list, original_browser));

  // Opening existing browser profile shouldn't open additional browser windows.
  webui::OpenNewWindowForProfile(original_profile);
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(original_browser, browser_list->GetLastActive());

  // Opening additional browser will add new window and activate it.
  Profile* additional_profile = CreateProfile();
  activation_observer =
      std::make_unique<ExpectBrowserActivationForProfile>(additional_profile);
  webui::OpenNewWindowForProfile(additional_profile);
  EXPECT_EQ(2u, browser_list->size());
  activation_observer->Wait();
  Browser* additional_browser = browser_list->GetLastActive();
  EXPECT_EQ(additional_profile, additional_browser->profile());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Await complete window creation to avoid interference with the next steps.
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(additional_browser));
#endif

// On Macs OpenNewWindowForProfile does not activate existing browser
// while non of the browser windows have focus. BrowserWindowCocoa::Show() got
// the same issue as BrowserWindowCocoa::Activate(), and execute call
// BrowserList::SetLastActive() directly. Not sure if it is a bug or desired
// behaviour.
#if !BUILDFLAG(IS_MAC)
  // Switch to original browser. Only LastActive should change.
  activation_observer =
      std::make_unique<ExpectBrowserActivationForProfile>(original_profile);
  webui::OpenNewWindowForProfile(original_profile);
  EXPECT_EQ(2u, browser_list->size());
  activation_observer->Wait();
  EXPECT_EQ(original_profile, browser_list->GetLastActive()->profile());
#endif
}

// The solo profile on Lacros is the main profile which can never be deleted.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfileHelperTest, DeleteSoleProfile) {
  content::TestWebUI web_ui;
  Browser* original_browser = browser();
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  base::FilePath original_browser_profile_path =
      original_browser->profile()->GetPath();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_TRUE(base::Contains(*browser_list, original_browser));
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  // Original browser will be closed, and browser with the new profile created.
  BrowserAddedObserver added_observer;
  webui::DeleteProfileAtPath(original_browser->profile()->GetPath(),
                             ProfileMetrics::DELETE_PROFILE_SETTINGS);
  ui_test_utils::WaitForBrowserToClose(original_browser);
  Browser* new_browser = added_observer.Wait();

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, browser_list->size());
  EXPECT_NE(original_browser_profile_path, new_browser->profile()->GetPath());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(ProfileHelperTest, DeleteActiveProfile) {
  content::TestWebUI web_ui;
  Browser* original_browser = browser();
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_TRUE(base::Contains(*browser_list, original_browser));
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  Profile* additional_profile = CreateProfile();
  EXPECT_EQ(2u, storage.GetNumberOfProfiles());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The original browser belongs to the main profile which can't be deleted.
  // Make the additional profile active and close the original browser.
  profiles::SwitchToProfile(additional_profile->GetPath(), false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, browser_list->size());
  CloseBrowserSynchronously(original_browser);
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(additional_profile, browser_list->get(0)->profile());
  // ProfileManager will switch active profile upon observing
  // BrowserListObserver::OnBrowserSetLastActive(). Wait until the event
  // is observed if the active profile has not switched to `additional_profile`
  // yet.
  bool wait_for_set_last_active_observed =
      ProfileManager::GetLastUsedProfileIfLoaded() != additional_profile;
  ui_test_utils::WaitForBrowserSetLastActive(browser_list->get(0),
                                             wait_for_set_last_active_observed);

  // Ensure the last active browser and the`LastUsedProfile` is set.
  EXPECT_EQ(chrome::FindLastActive(), browser_list->get(0));
  EXPECT_EQ(g_browser_process->profile_manager()->GetLastUsedProfileDir(),
            additional_profile->GetPath());

  // Original browser now belongs to the additional profile.
  original_browser = browser_list->get(0);
#endif

  // Original browser will be closed, and browser with the new profile created.
  webui::DeleteProfileAtPath(original_browser->profile()->GetPath(),
                             ProfileMetrics::DELETE_PROFILE_SETTINGS);
  ui_test_utils::WaitForBrowserToClose(original_browser);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, browser_list->size());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_TRUE(browser_list->get(0)->profile()->IsMainProfile());
#else
  EXPECT_EQ(additional_profile, browser_list->get(0)->profile());
#endif
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
}

class ProfileHelperTestWithDestroyProfile
    : public ProfileHelperTest,
      public testing::WithParamInterface<bool> {
 public:
  ProfileHelperTestWithDestroyProfile() {
    bool enable_destroy_profile = GetParam();
    if (enable_destroy_profile) {
      feature_list_.InitAndEnableFeature(
          features::kDestroyProfileOnBrowserClose);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kDestroyProfileOnBrowserClose);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/40945232): Fix this flaky test. Probably a timing issue.
IN_PROC_BROWSER_TEST_P(ProfileHelperTestWithDestroyProfile,
                       DISABLED_DeleteInactiveProfile) {
  content::TestWebUI web_ui;
  Browser* original_browser = browser();
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_TRUE(base::Contains(*browser_list, original_browser));
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  Profile* additional_profile = CreateProfile();
  EXPECT_EQ(2u, storage.GetNumberOfProfiles());

  base::FilePath additional_profile_dir = additional_profile->GetPath();
  bool destroy_profile =
      base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose);

  if (destroy_profile) {
    ProfileDeletionObserver observer;
    webui::DeleteProfileAtPath(additional_profile_dir,
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
    observer.Wait();
  } else {
    content::BrowsingDataRemoverCompletionInhibitor inhibitor(
        additional_profile->GetBrowsingDataRemover());
    webui::DeleteProfileAtPath(additional_profile_dir,
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
    inhibitor.BlockUntilNearCompletion();
    inhibitor.ContinueToCompletion();
  }

  EXPECT_EQ(1u, browser_list->size());
  EXPECT_TRUE(base::Contains(*browser_list, original_browser));
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  if (destroy_profile) {
    // Check that NukeProfileFromDisk() works correctly.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::Time start = base::Time::Now();
    while (base::PathExists(additional_profile_dir) &&
           base::Time::Now() - start < TestTimeouts::action_timeout()) {
      base::RunLoop().RunUntilIdle();
    }
    EXPECT_FALSE(base::PathExists(additional_profile_dir));
  }
}

#if BUILDFLAG(IS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(DestroyProfileOnBrowserClose,
                         ProfileHelperTestWithDestroyProfile,
                         testing::Values(false));
#else
INSTANTIATE_TEST_SUITE_P(DestroyProfileOnBrowserClose,
                         ProfileHelperTestWithDestroyProfile,
                         testing::Bool());
#endif  // BUILDFLAG(IS_CHROMEOS)
