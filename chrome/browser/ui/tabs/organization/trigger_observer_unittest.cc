// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger_observer.h"

#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/base/mojom/window_show_state.mojom.h"
// #include "content/public/test/browser_task_environment.h"
// #include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
// #include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
// #include "ui/base/page_transition_types.h"

namespace {
class AlwaysTrigger : public TriggerPolicy {
 public:
  AlwaysTrigger() = default;
  bool ShouldTrigger(float score) override { return true; }
};

std::unique_ptr<TabOrganizationTrigger> MakeTestTrigger() {
  return std::make_unique<TabOrganizationTrigger>(
      base::BindLambdaForTesting([](TabStripModel* tab_strip_model) -> float {
        return tab_strip_model->count();
      }),
      2.0f, std::make_unique<AlwaysTrigger>());
}
}  // namespace

class TabOrganizationTriggerObserverTest : public BrowserWithTestWindowTest {
 public:
  TabOrganizationTriggerObserverTest() = default;
  TabOrganizationTriggerObserverTest(
      const TabOrganizationTriggerObserverTest&) = delete;
  TabOrganizationTriggerObserverTest& operator=(
      const TabOrganizationTriggerObserverTest&) = delete;

  Browser* AddBrowser() {
    Browser::CreateParams native_params(profile_.get(), true);
    native_params.initial_show_state = ui::mojom::WindowShowState::kDefault;
    std::unique_ptr<Browser> browser =
        CreateBrowserWithTestWindowForParams(native_params);
    Browser* browser_ptr = browser.get();
    browsers_.emplace_back(std::move(browser));
    return browser_ptr;
  }

  content::WebContents* AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);

    content::WebContents* web_contents_ptr = web_contents.get();

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

    return web_contents_ptr;
  }

  TestingProfile* profile() { return profile_.get(); }
  TabOrganizationTriggerObserver* trigger_observer() {
    return trigger_observer_.get();
  }
  std::vector<raw_ptr<const Browser>> trigger_records() {
    return trigger_records_;
  }

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    trigger_observer_ = std::make_unique<TabOrganizationTriggerObserver>(
        base::BindRepeating(&TabOrganizationTriggerObserverTest::OnTrigger,
                            base::Unretained(this)),
        profile_.get(), MakeTestTrigger());
    trigger_records_ = std::vector<raw_ptr<const Browser>>();
  }
  void TearDown() override {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  void OnTrigger(const Browser* browser) {
    trigger_records_.push_back(browser);
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TabOrganizationTriggerObserver> trigger_observer_;
  std::vector<std::unique_ptr<Browser>> browsers_;
  std::vector<raw_ptr<const Browser>> trigger_records_;
};

TEST_F(TabOrganizationTriggerObserverTest, TriggersOnTabStripModelChange) {
  ASSERT_TRUE(trigger_records().empty());
  Browser* const browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  EXPECT_TRUE(trigger_records().empty());
  AddTabToBrowser(browser, 0);
  EXPECT_EQ(trigger_records().size(), 1u);
  EXPECT_EQ(trigger_records()[0], browser);
}
