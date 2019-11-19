// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/skia/include/core/SkColor.h"

using content::SiteInstance;
using content::WebContents;
using content::WebContentsTester;
using session_manager::SessionState;

class BrowserUnitTest : public BrowserWithTestWindowTest {
 public:
  BrowserUnitTest() {}
  ~BrowserUnitTest() override {}

  // Caller owns the memory.
  std::unique_ptr<WebContents> CreateTestWebContents() {
    return WebContentsTester::CreateTestWebContents(
        profile(), SiteInstance::Create(profile()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserUnitTest);
};

// Ensure crashed tabs are not reloaded when selected. crbug.com/232323
TEST_F(BrowserUnitTest, ReloadCrashedTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Start with a single foreground tab. |tab_strip_model| owns the memory.
  std::unique_ptr<WebContents> contents1 = CreateTestWebContents();
  content::WebContents* raw_contents1 = contents1.get();
  tab_strip_model->AppendWebContents(std::move(contents1), true);
  WebContentsTester::For(raw_contents1)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(raw_contents1)->TestSetIsLoading(false);
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_FALSE(raw_contents1->IsLoading());

  // Add a second tab in the background.
  std::unique_ptr<WebContents> contents2 = CreateTestWebContents();
  content::WebContents* raw_contents2 = contents2.get();
  tab_strip_model->AppendWebContents(std::move(contents2), false);
  WebContentsTester::For(raw_contents2)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(raw_contents2)->TestSetIsLoading(false);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_FALSE(raw_contents2->IsLoading());

  // Simulate the second tab crashing.
  raw_contents2->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(raw_contents2->IsCrashed());

  // Selecting the second tab does not cause a load or clear the crash.
  tab_strip_model->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  EXPECT_TRUE(tab_strip_model->IsTabSelected(1));
  EXPECT_FALSE(raw_contents2->IsLoading());
  EXPECT_TRUE(raw_contents2->IsCrashed());
}

// This tests a workaround which is not necessary on Mac.
// https://crbug.com/719230
#if defined(OS_MACOSX)
#define MAYBE_SetBackgroundColorForNewTab DISABLED_SetBackgroundColorForNewTab
#else
#define MAYBE_SetBackgroundColorForNewTab SetBackgroundColorForNewTab
#endif
TEST_F(BrowserUnitTest, MAYBE_SetBackgroundColorForNewTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  std::unique_ptr<WebContents> contents1 = CreateTestWebContents();
  content::WebContents* raw_contents1 = contents1.get();
  tab_strip_model->AppendWebContents(std::move(contents1), true);
  WebContentsTester::For(raw_contents1)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(raw_contents1)->TestSetIsLoading(false);

  raw_contents1->GetMainFrame()->GetView()->SetBackgroundColor(SK_ColorRED);

  // Add a second tab in the background.
  std::unique_ptr<WebContents> contents2 = CreateTestWebContents();
  content::WebContents* raw_contents2 = contents2.get();
  tab_strip_model->AppendWebContents(std::move(contents2), false);
  WebContentsTester::For(raw_contents2)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(raw_contents2)->TestSetIsLoading(false);

  tab_strip_model->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  ASSERT_TRUE(raw_contents2->GetMainFrame()->GetView()->GetBackgroundColor());
  EXPECT_EQ(SK_ColorRED,
            *raw_contents2->GetMainFrame()->GetView()->GetBackgroundColor());
}

// Ensure the print command gets disabled when a tab crashes.
TEST_F(BrowserUnitTest, DisablePrintOnCrashedTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  std::unique_ptr<WebContents> contents = CreateTestWebContents();
  content::WebContents* raw_contents = contents.get();
  tab_strip_model->AppendWebContents(std::move(contents), true);
  WebContentsTester::For(raw_contents)->NavigateAndCommit(GURL("about:blank"));

  CommandUpdater* command_updater = browser()->command_controller();

  EXPECT_FALSE(raw_contents->IsCrashed());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_PRINT));
  EXPECT_TRUE(chrome::CanPrint(browser()));

  raw_contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);

  EXPECT_TRUE(raw_contents->IsCrashed());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_PRINT));
  EXPECT_FALSE(chrome::CanPrint(browser()));
}

// Ensure the zoom-in and zoom-out commands get disabled when a tab crashes.
TEST_F(BrowserUnitTest, DisableZoomOnCrashedTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  std::unique_ptr<WebContents> contents = CreateTestWebContents();
  content::WebContents* raw_contents = contents.get();
  tab_strip_model->AppendWebContents(std::move(contents), true);
  WebContentsTester::For(raw_contents)->NavigateAndCommit(GURL("about:blank"));
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(raw_contents);
  EXPECT_TRUE(
      zoom_controller->SetZoomLevel(zoom_controller->GetDefaultZoomLevel()));

  CommandUpdater* command_updater = browser()->command_controller();

  EXPECT_TRUE(zoom_controller->IsAtDefaultZoom());
  EXPECT_FALSE(raw_contents->IsCrashed());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_ZOOM_PLUS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_ZOOM_MINUS));
  EXPECT_TRUE(chrome::CanZoomIn(raw_contents));
  EXPECT_TRUE(chrome::CanZoomOut(raw_contents));

  raw_contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);

  EXPECT_TRUE(raw_contents->IsCrashed());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_ZOOM_PLUS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_ZOOM_MINUS));
  EXPECT_FALSE(chrome::CanZoomIn(raw_contents));
  EXPECT_FALSE(chrome::CanZoomOut(raw_contents));
}

// Tests that Browser::Create creates a guest session browser for OTR profile
// only.
TEST_F(BrowserUnitTest, CreateGuestSessionBrowser) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.SetGuestSession();
  otr_profile_builder.BuildIncognito(test_profile.get());

  // Try creating a browser in original guest profile - it should fail.
  std::unique_ptr<Browser> browser(
      Browser::Create(Browser::CreateParams(test_profile.get(), false)));
  EXPECT_FALSE(browser);

  // Creating a browser in OTR guest profile should succeed.
  Browser::CreateParams off_the_record_create_params(
      test_profile->GetOffTheRecordProfile(), false);
  std::unique_ptr<BrowserWindow> test_window(CreateBrowserWindow());
  off_the_record_create_params.window = test_window.get();
  std::unique_ptr<Browser> otr_browser(
      Browser::Create(off_the_record_create_params));
  EXPECT_TRUE(otr_browser);
}

TEST_F(BrowserUnitTest, CreateBrowserFailsIfProfileDisallowsBrowserWindows) {
  TestingProfile::Builder profile_builder;
  profile_builder.DisallowBrowserWindows();
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.DisallowBrowserWindows();
  otr_profile_builder.BuildIncognito(test_profile.get());

  // Verify creating browser fails in both original and OTR version of the
  // profile.
  std::unique_ptr<Browser> browser(
      Browser::Create(Browser::CreateParams(test_profile.get(), false)));
  EXPECT_FALSE(browser);
  std::unique_ptr<Browser> otr_browser(Browser::Create(
      Browser::CreateParams(test_profile->GetOffTheRecordProfile(), false)));
  EXPECT_FALSE(otr_browser);
}

// Tests BrowserCreate() when Incognito mode is disabled.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeDisabled) {
  TestingProfile::Builder profile_builder;
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  IncognitoModePrefs::SetAvailability(test_profile->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);

  // Creating a browser window in OTR profile should fail if incognito is
  // disabled.
  std::unique_ptr<Browser> otr_browser(Browser::Create(
      Browser::CreateParams(test_profile->GetOffTheRecordProfile(), false)));
  EXPECT_FALSE(otr_browser);

  // Verify creating a browser in the original profile succeeds.
  Browser::CreateParams create_params(test_profile.get(), false);
  std::unique_ptr<BrowserWindow> test_window(CreateBrowserWindow());
  create_params.window = test_window.get();
  std::unique_ptr<Browser> test_browser(Browser::Create(create_params));
  EXPECT_TRUE(test_browser);
}

// Tests BrowserCreate() when Incognito mode is forced.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeForced) {
  TestingProfile::Builder profile_builder;
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  IncognitoModePrefs::SetAvailability(test_profile->GetPrefs(),
                                      IncognitoModePrefs::FORCED);

  // Creating a browser window in the original profile should fail if incognito
  // is forced.
  std::unique_ptr<Browser> browser(
      Browser::Create(Browser::CreateParams(test_profile.get(), false)));
  EXPECT_FALSE(browser);

  // Creating a browser in OTR test profile should succeed.
  Browser::CreateParams off_the_record_create_params(
      test_profile->GetOffTheRecordProfile(), false);
  std::unique_ptr<BrowserWindow> test_window(CreateBrowserWindow());
  off_the_record_create_params.window = test_window.get();
  std::unique_ptr<Browser> otr_browser(
      Browser::Create(off_the_record_create_params));
  EXPECT_TRUE(otr_browser);
}

// Tests BrowserCreate() with not restrictions on incognito mode.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeEnabled) {
  TestingProfile::Builder profile_builder;
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();

  ASSERT_EQ(IncognitoModePrefs::ENABLED,
            IncognitoModePrefs::GetAvailability(test_profile->GetPrefs()));

  // Creating a browser in the original test profile should succeed.
  Browser::CreateParams create_params(test_profile.get(), false);
  std::unique_ptr<BrowserWindow> test_window(CreateBrowserWindow());
  create_params.window = test_window.get();
  std::unique_ptr<Browser> test_browser(Browser::Create(create_params));
  EXPECT_TRUE(test_browser);

  // Creating a browser in OTR test profile should succeed.
  Browser::CreateParams off_the_record_create_params(
      test_profile->GetOffTheRecordProfile(), false);
  std::unique_ptr<BrowserWindow> otr_test_window(CreateBrowserWindow());
  off_the_record_create_params.window = otr_test_window.get();
  std::unique_ptr<Browser> otr_browser(
      Browser::Create(off_the_record_create_params));
  EXPECT_TRUE(otr_browser);
}

#if defined(OS_CHROMEOS)
TEST_F(BrowserUnitTest, CreateBrowserDuringKioskSplashScreen) {
  session_manager::SessionManager session_manager;

  // Setting up user manager state to be in kiosk mode:
  // Creating a new user manager.
  chromeos::FakeChromeUserManager* user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager manager{
      std::unique_ptr<user_manager::UserManager>(user_manager)};
  const user_manager::User* user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail("fake_user@test"));
  user_manager->LoginUser(user->GetAccountId());

  TestingProfile profile;
  Browser::CreateParams create_params(&profile, false);

  std::unique_ptr<BrowserWindow> window1(CreateBrowserWindow());
  create_params.window = window1.get();
  session_manager.SetSessionState(SessionState::LOGIN_PRIMARY);
  std::unique_ptr<Browser> test_browser(Browser::Create(create_params));
  // Browser should not be created during login session state.
  EXPECT_FALSE(test_browser);

  std::unique_ptr<BrowserWindow> window2(CreateBrowserWindow());
  create_params.window = window2.get();
  session_manager.SetSessionState(SessionState::ACTIVE);
  std::unique_ptr<Browser> test_browser2(Browser::Create(create_params));
  // Normal flow, creation succeeds.
  EXPECT_TRUE(test_browser2);
}
#endif

class BrowserBookmarkBarTest : public BrowserWithTestWindowTest {
 public:
  BrowserBookmarkBarTest() {}
  ~BrowserBookmarkBarTest() override {}

 protected:
  BookmarkBar::State window_bookmark_bar_state() const {
    return static_cast<BookmarkBarStateTestBrowserWindow*>(browser()->window())
        ->bookmark_bar_state();
  }

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    static_cast<BookmarkBarStateTestBrowserWindow*>(browser()->window())
        ->set_browser(browser());
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::make_unique<BookmarkBarStateTestBrowserWindow>();
  }

 private:
  class BookmarkBarStateTestBrowserWindow : public TestBrowserWindow {
   public:
    BookmarkBarStateTestBrowserWindow()
        : browser_(NULL), bookmark_bar_state_(BookmarkBar::HIDDEN) {}
    ~BookmarkBarStateTestBrowserWindow() override {}

    void set_browser(Browser* browser) { browser_ = browser; }

    BookmarkBar::State bookmark_bar_state() const {
      return bookmark_bar_state_;
    }

   private:
    // TestBrowserWindow:
    void BookmarkBarStateChanged(
        BookmarkBar::AnimateChangeType change_type) override {
      bookmark_bar_state_ = browser_->bookmark_bar_state();
      TestBrowserWindow::BookmarkBarStateChanged(change_type);
    }

    void OnActiveTabChanged(content::WebContents* old_contents,
                            content::WebContents* new_contents,
                            int index,
                            int reason) override {
      bookmark_bar_state_ = browser_->bookmark_bar_state();
      TestBrowserWindow::OnActiveTabChanged(old_contents, new_contents, index,
                                            reason);
    }

    Browser* browser_;  // Weak ptr.
    BookmarkBar::State bookmark_bar_state_;

    DISALLOW_COPY_AND_ASSIGN(BookmarkBarStateTestBrowserWindow);
  };

  DISALLOW_COPY_AND_ASSIGN(BrowserBookmarkBarTest);
};

// Ensure bookmark bar states in Browser and BrowserWindow are in sync after
// Browser::ActiveTabChanged() calls BrowserWindow::OnActiveTabChanged().
TEST_F(BrowserBookmarkBarTest, StateOnActiveTabChanged) {
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  ASSERT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  GURL ntp_url("chrome://newtab");
  GURL non_ntp_url("http://foo");

  // Open a tab to NTP.
  AddTab(browser(), ntp_url);
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  // Navigate 1st tab to a non-NTP URL.
  NavigateAndCommitActiveTab(non_ntp_url);
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  // Open a tab to NTP at index 0.
  AddTab(browser(), ntp_url);
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  // Activate the 2nd tab which is non-NTP.
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::HIDDEN, window_bookmark_bar_state());

  // Toggle bookmark bar while 2nd tab (non-NTP) is active.
  chrome::ToggleBookmarkBar(browser());
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::SHOW, window_bookmark_bar_state());

  // Activate the 1st tab which is NTP.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::SHOW, window_bookmark_bar_state());

  // Activate the 2nd tab which is non-NTP.
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  EXPECT_EQ(BookmarkBar::SHOW, window_bookmark_bar_state());
}
