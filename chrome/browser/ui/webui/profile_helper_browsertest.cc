// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"

namespace {

// An observer that returns back to test code after a new profile is
// initialized.
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::Bind(&UnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(new_path);
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
  Profile* profile_;
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
  Browser* browser_;
  base::RunLoop run_loop_;
};

}  // namespace

using ProfileHelperTest = InProcessBrowserTest;

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

  // Open additional browser will add new window and activates it.
  Profile* additional_profile = CreateProfile();
  activation_observer =
      std::make_unique<ExpectBrowserActivationForProfile>(additional_profile);
  webui::OpenNewWindowForProfile(additional_profile);
  EXPECT_EQ(2u, browser_list->size());
  activation_observer->Wait();
  EXPECT_EQ(additional_profile, browser_list->GetLastActive()->profile());

// On Macs OpenNewWindowForProfile does not activate existing browser
// while non of the browser windows have focus. BrowserWindowCocoa::Show() got
// the same issue as BrowserWindowCocoa::Activate(), and execute call
// BrowserList::SetLastActive() directly. Not sure if it is a bug or desired
// behaviour.
#if !defined(OS_MACOSX)
  // Switch to original browser. Only LastActive should change.
  activation_observer =
      std::make_unique<ExpectBrowserActivationForProfile>(original_profile);
  webui::OpenNewWindowForProfile(original_profile);
  EXPECT_EQ(2u, browser_list->size());
  activation_observer->Wait();
  EXPECT_EQ(original_profile, browser_list->GetLastActive()->profile());
#endif
}

IN_PROC_BROWSER_TEST_F(ProfileHelperTest, DeleteSoleProfile) {
  content::TestWebUI web_ui;
  Browser* original_browser = browser();
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

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

  EXPECT_EQ(1u, browser_list->size());
  EXPECT_FALSE(base::Contains(*browser_list, original_browser));
  EXPECT_NE(new_browser, original_browser);
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
}

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

  // Original browser will be closed, and browser with the new profile created.
  webui::DeleteProfileAtPath(original_browser->profile()->GetPath(),
                             ProfileMetrics::DELETE_PROFILE_SETTINGS);
  ui_test_utils::WaitForBrowserToClose(original_browser);

  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(additional_profile, browser_list->get(0)->profile());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
}

IN_PROC_BROWSER_TEST_F(ProfileHelperTest, DeleteInactiveProfile) {
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

  content::BrowsingDataRemoverCompletionInhibitor inhibitor(
      content::BrowserContext::GetBrowsingDataRemover(additional_profile));
  webui::DeleteProfileAtPath(additional_profile->GetPath(),
                             ProfileMetrics::DELETE_PROFILE_SETTINGS);
  inhibitor.BlockUntilNearCompletion();
  inhibitor.ContinueToCompletion();

  EXPECT_EQ(1u, browser_list->size());
  EXPECT_TRUE(base::Contains(*browser_list, original_browser));
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
}
