// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/browser_instant_controller.h"

#include <stddef.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

namespace chrome {

namespace {

class BrowserInstantControllerTest : public InstantUnitTestBase {
 protected:
  friend class FakeWebContentsObserver;

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            test_url_loader_factory())}};
  }
};

struct TabReloadTestCase {
  const char* description;
  const char* start_url;
  bool start_in_instant_process;
  bool end_in_ntp;
};

// Test cases for when Google is the initial, but not final provider.
const TabReloadTestCase kTabReloadTestCasesFinalProviderNotGoogle[] = {
    {"NTP", chrome::kChromeUINewTabPageURL, false, true},
    {"Remote SERP", "https://www.google.com/url?bar=search+terms", false,
     false},
    {"Other NTP", "https://bar.com/newtab", false, false}};

class FakeWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit FakeWebContentsObserver(content::WebContents* contents)
      : WebContentsObserver(contents),
        contents_(contents),
        did_start_observer_(contents),
        url_(contents->GetURL()),
        num_reloads_(0) {}

  void DidStartNavigation(content::NavigationHandle* navigation) override {
    if (navigation->GetReloadType() == content::ReloadType::NONE)
      return;
    if (*url_ == navigation->GetURL())
      num_reloads_++;
    current_url_ = navigation->GetURL();
  }

  const GURL& url() const { return *url_; }

  const GURL& current_url() const { return contents_->GetURL(); }

  int num_reloads() const { return num_reloads_; }

  bool can_go_back() const { return contents_->GetController().CanGoBack(); }

  void WaitForNavigationStart() { did_start_observer_.Wait(); }

 protected:
  friend class BrowserInstantControllerTest;
  FRIEND_TEST_ALL_PREFIXES(BrowserInstantControllerTest,
                           DefaultSearchProviderChanged);
  FRIEND_TEST_ALL_PREFIXES(BrowserInstantControllerTest, GoogleBaseURLUpdated);

 private:
  raw_ptr<content::WebContents> contents_;
  content::DidStartNavigationObserver did_start_observer_;
  const raw_ref<const GURL> url_;
  GURL current_url_;
  int num_reloads_;
};

TEST_F(BrowserInstantControllerTest, DefaultSearchProviderChanged) {
  size_t num_tests = std::size(kTabReloadTestCasesFinalProviderNotGoogle);
  std::vector<std::unique_ptr<FakeWebContentsObserver>> observers;
  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test =
        kTabReloadTestCasesFinalProviderNotGoogle[i];
    AddTab(browser(), GURL(test.start_url));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Validate initial instant state.
    EXPECT_EQ(test.start_in_instant_process,
              instant_service_->IsInstantProcess(
                  contents->GetPrimaryMainFrame()->GetProcess()->GetID()))
        << test.description;

    // Setup an observer to verify reload or absence thereof.
    observers.push_back(std::make_unique<FakeWebContentsObserver>(contents));
  }

  SetUserSelectedDefaultSearchProvider("https://bar.com/");

  for (size_t i = 0; i < num_tests; ++i) {
    FakeWebContentsObserver* observer = observers[i].get();
    const TabReloadTestCase& test =
        kTabReloadTestCasesFinalProviderNotGoogle[i];

    // Ensure only the expected tabs(contents) reloaded.
    // RunUntilIdle() ensures that tasks posted by TabReloader::Reload run.
    base::RunLoop().RunUntilIdle();
    if (observer->web_contents()->IsLoading()) {
      // Ensure that we get DidStartNavigation, which can be dispatched
      // asynchronously.
      observer->WaitForNavigationStart();
    }

    if (test.end_in_ntp) {
      EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), observer->current_url())
          << test.description;
    }
  }
}

}  // namespace

}  // namespace chrome
