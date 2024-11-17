// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

class TabModelTest : public testing::Test {
 public:
  TabModelTest() = default;
  TabModelTest(const TabModelTest&) = delete;
  TabModelTest& operator=(const TabModelTest&) = delete;
  ~TabModelTest() override = default;

  TestingProfile* profile() { return &profile_; }

  void AppendTab(TabStripModel& tab_strip_model) {
    tab_strip_model.AppendTab(
        std::make_unique<tabs::TabModel>(
            content::WebContentsTester::CreateTestWebContents(profile(),
                                                              nullptr),
            &tab_strip_model),
        true);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  tabs::PreventTabFeatureInitialization prevent_;
};

TEST_F(TabModelTest, TabModelDidInsert) {
  // Create a source tab strip.
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_src(&delegate, profile());
  AppendTab(tab_strip_src);
  AppendTab(tab_strip_src);

  // Detach the first tab.
  std::unique_ptr<TabModel> tab_model =
      tab_strip_src.DetachTabAtForInsertion(0);

  // Assert the subscription is notified when the detached tab is inserted into
  // a new tab strip.
  TabStripModel tab_strip_dst(&delegate, profile());
  base::MockCallback<TabInterface::DidInsertCallback> did_insert_callback;
  EXPECT_CALL(did_insert_callback, Run).Times(1);
  base::CallbackListSubscription subscription =
      tab_model->RegisterDidInsert(did_insert_callback.Get());
  tab_strip_dst.InsertDetachedTabAt(0, std::move(tab_model),
                                    AddTabTypes::ADD_NONE);
  testing::Mock::VerifyAndClearExpectations(&did_insert_callback);

  // Assert the subscription is notified again when the detached tab is
  // re-inserted into its original tab strip.
  tab_model = tab_strip_dst.DetachTabAtForInsertion(0);
  EXPECT_CALL(did_insert_callback, Run).Times(1);
  tab_strip_src.InsertDetachedTabAt(0, std::move(tab_model),
                                    AddTabTypes::ADD_NONE);
}

}  // namespace tabs
