// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include <stddef.h>

#include <array>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "chrome/browser/search/instant_browsertest_base.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
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

class BrowserInstantControllerTest : public InstantBrowserTestBase {
 protected:
  friend class FakeWebContentsObserver;
};

struct TabReloadTestCase {
  const char* description;
  const char* start_url;
  bool start_in_instant_process;
  bool end_in_ntp;
};

// Test cases for when Google is the initial, but not final provider.
const auto kTabReloadTestCasesFinalProviderNotGoogle =
    std::to_array<TabReloadTestCase>({
        {"NTP", chrome::kChromeUINewTabPageURL, false, true},
        {"Remote SERP", "https://www.google.com/url?bar=search+terms", false,
         false},
        {"Other NTP", "https://bar.com/newtab", false, false},
    });

class FakeWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit FakeWebContentsObserver(content::WebContents* contents)
      : WebContentsObserver(contents),
        contents_(contents),
        did_start_observer_(contents),
        url_(contents->GetURL()) {}

  void DidStartNavigation(content::NavigationHandle* navigation) override {
    if (navigation->GetReloadType() == content::ReloadType::NONE) {
      return;
    }
    if (*url_ == navigation->GetURL()) {
      num_reloads_++;
    }
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
  int num_reloads_ = 0;
};

// TODO(crbug.com/428088800): Test is flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DefaultSearchProviderChanged DISABLED_DefaultSearchProviderChanged
#else
#define MAYBE_DefaultSearchProviderChanged DefaultSearchProviderChanged
#endif
IN_PROC_BROWSER_TEST_F(BrowserInstantControllerTest,
                       MAYBE_DefaultSearchProviderChanged) {
  size_t num_tests = std::size(kTabReloadTestCasesFinalProviderNotGoogle);
  std::vector<std::unique_ptr<FakeWebContentsObserver>> observers;
  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test =
        kTabReloadTestCasesFinalProviderNotGoogle[i];
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(test.start_url),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Validate initial instant state.
    EXPECT_EQ(
        test.start_in_instant_process,
        instant_service_->IsInstantProcess(
            contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()))
        << test.description;

    // Setup an observer to verify reload or absence thereof.
    observers.push_back(std::make_unique<FakeWebContentsObserver>(contents));

    // Tests for which tabs finish in the NTP should start in the default NTP
    // before the provider is changed.
    if (test.end_in_ntp) {
      EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL),
                observers.back()->current_url());
    }
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
      EXPECT_EQ(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                observer->current_url())
          << test.description;
    }
  }
}

}  // namespace

}  // namespace chrome
