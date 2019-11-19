// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/views/test/widget_test.h"

CocoaProfileTest::CocoaProfileTest()
    : task_environment_(new content::BrowserTaskEnvironment),
      views_helper_(std::make_unique<ChromeTestViewsDelegate>()),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      profile_(nullptr) {}

CocoaProfileTest::~CocoaProfileTest() {
  // Delete the testing profile on the UI thread. But first release the
  // browser, since it may trigger accesses to the profile upon destruction.
  browser_.reset();

  base::RunLoop().RunUntilIdle();

  // Some services created on the TestingProfile require deletion on the UI
  // thread. If the scoper in TestingBrowserProcess, owned by ChromeTestSuite,
  // were to delete the ProfileManager, the UI thread would at that point no
  // longer exist.
  TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);

  // Make sure any pending tasks run before we destroy other threads.
  base::RunLoop().RunUntilIdle();
}

void CocoaProfileTest::SetUp() {
  CocoaTest::SetUp();

  ASSERT_TRUE(profile_manager_.SetUp());

  // Always fake out the Gaia service to avoid issuing network requests.
  TestingProfile::TestingFactories testing_factories = {
      {ChromeSigninClientFactory::GetInstance(),
       base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                           &test_url_loader_factory_)}};

  profile_ = profile_manager_.CreateTestingProfile(
      "Person 1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16("Person 1"), 0, std::string(),
      std::move(testing_factories));
  ASSERT_TRUE(profile_);

  // TODO(shess): These are needed in case someone creates a browser
  // window off of browser_.  pkasting indicates that other
  // platforms use a stub |BrowserWindow| and thus don't need to do
  // this.
  // http://crbug.com/39725
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_,
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));

  // Configure the GaiaCookieManagerService to return no accounts.
  signin::SetListAccountsResponseHttpNotFound(&test_url_loader_factory_);

  profile_->CreateBookmarkModel(true);
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile_));

  browser_.reset(CreateBrowser());
  ASSERT_TRUE(browser_.get());
}

void CocoaProfileTest::TearDown() {
  if (browser_.get() && browser_->window())
    CloseBrowserWindow();

  CocoaTest::TearDown();
}

void CocoaProfileTest::CloseBrowserWindow() {
  // Check to make sure a window was actually created.
  DCHECK(browser_->window());
  browser_->tab_strip_model()->CloseAllTabs();
  chrome::CloseWindow(browser_.get());

  // |browser_| will be deleted by its frame.
  Browser* browser = browser_.release();
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  // Wait for the close to happen so that this method is synchronous.
  views::test::WidgetDestroyedWaiter waiter(browser_view->frame());
  waiter.Wait();
}

Browser* CocoaProfileTest::CreateBrowser() {
  return new Browser(Browser::CreateParams(profile(), true));
}
