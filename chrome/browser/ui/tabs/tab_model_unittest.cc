// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using RepeatingTabCallback =
    base::MockCallback<base::RepeatingCallback<void(tabs::TabInterface*)>>;
}

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
        /*foreground=*/true);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
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

TEST_F(TabModelTest, IsSelected) {
  // Create a source tab strip.
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  AppendTab(tab_strip);

  // Right now, the second tab should be selected.
  tabs::TabInterface* tab0 = tab_strip.GetTabAtIndex(0);
  tabs::TabInterface* tab1 = tab_strip.GetTabAtIndex(1);
  EXPECT_FALSE(tab0->IsSelected());
  EXPECT_TRUE(tab1->IsSelected());

  // Select both the first tab, too.
  tab_strip.SelectTabAt(0);
  EXPECT_TRUE(tab0->IsSelected());
  EXPECT_TRUE(tab1->IsSelected());

  // Deselect the second tab.
  tab_strip.DeselectTabAt(1);
  EXPECT_TRUE(tab0->IsSelected());
  EXPECT_FALSE(tab1->IsSelected());
}

TEST_F(TabModelTest, HandleAndSessionIdAreMapped) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  tabs::TabModel* tab_model =
      static_cast<tabs::TabModel*>(tab_strip.GetTabAtIndex(0));

  // The handle should be valid.
  EXPECT_NE(tab_model->GetHandle().raw_value(), tabs::TabHandle::NullValue);

  // The factory should be able to map the handle back to the session ID.
  auto* factory = &tabs::SessionMappedTabHandleFactory::GetInstance();
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab_model->GetContents());
  EXPECT_EQ(session_tab_helper->session_id().id(),
            factory->GetSessionIdForHandle(tab_model->GetHandle().raw_value()));
}

TEST_F(TabModelTest, DiscardContentsUpdatesSessionIdMapping) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  tabs::TabModel* tab_model =
      static_cast<tabs::TabModel*>(tab_strip.GetTabAtIndex(0));
  auto* factory = &tabs::SessionMappedTabHandleFactory::GetInstance();

  const int32_t original_session_id =
      sessions::SessionTabHelper::FromWebContents(tab_model->GetContents())
          ->session_id()
          .id();
  EXPECT_EQ(original_session_id,
            factory->GetSessionIdForHandle(tab_model->GetHandle().raw_value()));

  // Discard the contents and replace it with a new WebContents.
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  CreateSessionServiceTabHelper(new_contents.get());

  const int32_t new_session_id =
      sessions::SessionTabHelper::FromWebContents(new_contents.get())
          ->session_id()
          .id();
  ASSERT_NE(original_session_id, new_session_id);
  tab_model->DiscardContents(std::move(new_contents));

  EXPECT_EQ(new_session_id,
            factory->GetSessionIdForHandle(tab_model->GetHandle().raw_value()));
  EXPECT_EQ(tab_model->GetHandle().raw_value(),
            factory->GetHandleForSessionId(new_session_id));
  EXPECT_EQ(TabHandle::Null().raw_value(),
            factory->GetHandleForSessionId(original_session_id));
}

TEST_F(TabModelTest, SplitViewVisibleAndActiveCallbacks) {
  // Create a source tab strip with 3 tabs.
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  AppendTab(tab_strip);
  AppendTab(tab_strip);
  ASSERT_EQ(3, tab_strip.count());

  // Create a split with the first two tabs, then activate the tab outside the
  // split.
  tab_strip.ActivateTabAt(0);
  tab_strip.AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                          split_tabs::SplitTabCreatedSource::kToolbarButton);
  tab_strip.ActivateTabAt(2);

  // Set up subscriptions for activation. Note that visibility is tested in
  // browser tests.
  std::vector<base::CallbackListSubscription> subscriptions;

  RepeatingTabCallback did_tab_0_activate_callback;
  EXPECT_CALL(did_tab_0_activate_callback, Run).Times(1);
  subscriptions.push_back(tab_strip.GetTabAtIndex(0)->RegisterDidActivate(
      did_tab_0_activate_callback.Get()));

  RepeatingTabCallback did_tab_1_activate_callback;
  EXPECT_CALL(did_tab_1_activate_callback, Run).Times(0);
  subscriptions.push_back(tab_strip.GetTabAtIndex(1)->RegisterDidActivate(
      did_tab_1_activate_callback.Get()));

  // Activate a tab in the split and confirm expectations.
  tab_strip.ActivateTabAt(0);
  testing::Mock::VerifyAndClearExpectations(&did_tab_0_activate_callback);
  testing::Mock::VerifyAndClearExpectations(&did_tab_1_activate_callback);
}

}  // namespace tabs
