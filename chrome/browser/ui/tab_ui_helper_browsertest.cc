// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_ui_helper.h"

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/models/image_model.h"

namespace {
class MockTabUiHelperSubscriber {
 public:
  explicit MockTabUiHelperSubscriber(TabUIHelper* tab_ui_helper) {
    title_change_subscription_ = tab_ui_helper->AddTitleUpdatedCallback(
        base::BindRepeating(&::MockTabUiHelperSubscriber::OnTitleChange,
                            base::Unretained(this)));
  }
  ~MockTabUiHelperSubscriber() = default;

  MOCK_METHOD(void, OnTitleChange, (std::u16string updated_title));

 private:
  base::CallbackListSubscription title_change_subscription_;
};
}  // namespace

class TabUIHelperBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(TabUIHelperBrowserTest, TitleChangeIsNotified) {
  ASSERT_NE(ui_test_utils::NavigateToURL(
                browser(), embedded_test_server()->GetURL("/title2.html")),
            nullptr);
  tabs::TabInterface* const tab_interface =
      browser()->tab_strip_model()->GetActiveTab();
  TabUIHelper* const tab_ui_helper =
      tab_interface->GetTabFeatures()->tab_ui_helper();
  EXPECT_EQ(tab_ui_helper->GetTitle(), u"Title Of Awesomeness");
  auto title_change_waiter =
      std::make_unique<MockTabUiHelperSubscriber>(tab_ui_helper);
  EXPECT_CALL(*title_change_waiter,
              OnTitleChange(std::u16string(u"Title Of More Awesomeness")));
  ASSERT_NE(ui_test_utils::NavigateToURL(
                browser(), embedded_test_server()->GetURL("/title3.html")),
            nullptr);
}

class TabUIHelperWithPrerenderingTest : public InProcessBrowserTest {
 public:
  TabUIHelperWithPrerenderingTest()
      : prerender_test_helper_(base::BindRepeating(
            &TabUIHelperWithPrerenderingTest::GetWebContents,
            base::Unretained(this))) {}
  ~TabUIHelperWithPrerenderingTest() override = default;
  TabUIHelperWithPrerenderingTest(const TabUIHelperWithPrerenderingTest&) =
      delete;
  TabUIHelperWithPrerenderingTest& operator=(
      const TabUIHelperWithPrerenderingTest&) = delete;

  void SetUp() override {
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
};

IN_PROC_BROWSER_TEST_F(TabUIHelperWithPrerenderingTest,
                       ShouldNotAffectTabUIHelperOnPrerendering) {
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerender_url =
      embedded_test_server()->GetURL("/favicon/title2_with_favicon.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  TabUIHelper* const tab_ui_helper = browser()
                                         ->tab_strip_model()
                                         ->GetActiveTab()
                                         ->GetTabFeatures()
                                         ->tab_ui_helper();
  const std::u16string primary_title = tab_ui_helper->GetTitle();
  const ui::ImageModel primary_favicon = tab_ui_helper->GetFavicon();
  const bool primary_should_hide_throbber = tab_ui_helper->ShouldHideThrobber();

  // Set |create_by_session_restore_| to true to check if the value is changed
  // after prerendering. It should not be changed because DidStopLoading is not
  // called during the prerendering.
  tab_ui_helper->set_created_by_session_restore(true);

  // Prerender to another site.
  prerender_test_helper().AddPrerender(prerender_url);

  // Check if the prerendering doesn't affect the returned values of
  // TabUIHelper.
  EXPECT_EQ(primary_title, tab_ui_helper->GetTitle());
  EXPECT_EQ(primary_favicon, tab_ui_helper->GetFavicon());
  EXPECT_EQ(primary_should_hide_throbber, tab_ui_helper->ShouldHideThrobber());
  // is_created_by_session_restore_for_testing should return true because
  // DidStopLoading is not called.
  EXPECT_TRUE(tab_ui_helper->is_created_by_session_restore_for_testing());

  // Activate the prerendered page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);

  // Check if new values are different from the previous primary values after
  // activating the prerendered page.
  EXPECT_NE(primary_title, tab_ui_helper->GetTitle());
  EXPECT_FALSE(primary_favicon == tab_ui_helper->GetFavicon());
  EXPECT_FALSE(tab_ui_helper->ShouldHideThrobber());
}
