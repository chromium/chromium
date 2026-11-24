// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using RepeatingTabCallback =
    base::MockCallback<base::RepeatingCallback<void(tabs::TabInterface*)>>;
using SplitChangedCallbackMock = testing::StrictMock<
    base::MockCallback<tabs::TabInterface::SplitChangedCallback>>;
}  // namespace

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

TEST_F(TabModelTest, DetachTabModelGetBrowser) {
  MockBrowserWindowInterface bwi;
  TestTabStripModelDelegate delegate;
  delegate.SetBrowserWindowInterface(&bwi);

  // Create a source tab strip.
  TabStripModel tab_strip_src(&delegate, profile());
  AppendTab(tab_strip_src);
  AppendTab(tab_strip_src);

  // Detach the first tab.
  std::unique_ptr<TabModel> tab_model =
      tab_strip_src.DetachTabAtForInsertion(0);
  TabModel* tab_model_ptr = tab_model.get();

  // Verify GetBrowserWindowInterface returns nullptr.
  EXPECT_EQ(nullptr, tab_model_ptr->GetBrowserWindowInterface());

  // Attach to a destination tab strip.
  TabStripModel tab_strip_dst(&delegate, profile());
  tab_strip_dst.InsertDetachedTabAt(0, std::move(tab_model),
                                    AddTabTypes::ADD_NONE);

  // Verify GetBrowserWindowInterface returns the browser interface.
  EXPECT_EQ(&bwi, tab_model_ptr->GetBrowserWindowInterface());
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

// Verifies that when the active tab changes, only the newly-activated tab is
// notified that it entered the foreground. This exercises the direct
// notification from TabStripModel, and guards against both missing and spurious
// activation notifications.
TEST_F(TabModelTest, DidActivateNotifiesOnlyNewlyActiveTab) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  AppendTab(tab_strip);
  AppendTab(tab_strip);
  ASSERT_EQ(3, tab_strip.count());

  // Establish a known starting point: the last tab is active.
  tab_strip.ActivateTabAt(2);

  std::vector<base::CallbackListSubscription> subscriptions;

  // The tab we are about to activate should be notified exactly once, with
  // itself as the argument.
  RepeatingTabCallback did_tab_0_activate_callback;
  EXPECT_CALL(did_tab_0_activate_callback, Run(tab_strip.GetTabAtIndex(0)))
      .Times(1);
  subscriptions.push_back(tab_strip.GetTabAtIndex(0)->RegisterDidActivate(
      did_tab_0_activate_callback.Get()));

  // A tab that is not becoming active (including the one leaving the
  // foreground) must not be notified.
  RepeatingTabCallback did_tab_1_activate_callback;
  EXPECT_CALL(did_tab_1_activate_callback, Run).Times(0);
  subscriptions.push_back(tab_strip.GetTabAtIndex(1)->RegisterDidActivate(
      did_tab_1_activate_callback.Get()));

  RepeatingTabCallback did_tab_2_activate_callback;
  EXPECT_CALL(did_tab_2_activate_callback, Run).Times(0);
  subscriptions.push_back(tab_strip.GetTabAtIndex(2)->RegisterDidActivate(
      did_tab_2_activate_callback.Get()));

  tab_strip.ActivateTabAt(0);

  testing::Mock::VerifyAndClearExpectations(&did_tab_0_activate_callback);
  testing::Mock::VerifyAndClearExpectations(&did_tab_1_activate_callback);
  testing::Mock::VerifyAndClearExpectations(&did_tab_2_activate_callback);
}

// Verifies that each activation delivers exactly one foreground notification to
// the tab becoming active, across repeated active-tab changes.
TEST_F(TabModelTest, DidActivateNotifiesOnEachActivation) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  AppendTab(tab_strip);
  ASSERT_EQ(2, tab_strip.count());

  tab_strip.ActivateTabAt(0);

  std::vector<base::CallbackListSubscription> subscriptions;

  RepeatingTabCallback did_tab_0_activate_callback;
  RepeatingTabCallback did_tab_1_activate_callback;
  subscriptions.push_back(tab_strip.GetTabAtIndex(0)->RegisterDidActivate(
      did_tab_0_activate_callback.Get()));
  subscriptions.push_back(tab_strip.GetTabAtIndex(1)->RegisterDidActivate(
      did_tab_1_activate_callback.Get()));

  // Switch back and forth; each tab should be notified once per time it becomes
  // active. Re-activating the already-active tab should not notify anyone.
  EXPECT_CALL(did_tab_1_activate_callback, Run).Times(1);
  tab_strip.ActivateTabAt(1);
  tab_strip.ActivateTabAt(1);
  testing::Mock::VerifyAndClearExpectations(&did_tab_1_activate_callback);

  EXPECT_CALL(did_tab_0_activate_callback, Run).Times(1);
  tab_strip.ActivateTabAt(0);
  testing::Mock::VerifyAndClearExpectations(&did_tab_0_activate_callback);
}

TEST_F(TabModelTest, TabModelBlockedStateChanged) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  tabs::TabModel* const tab_model =
      static_cast<tabs::TabModel*>(tab_strip.GetTabAtIndex(0));

  base::MockCallback<TabInterface::BlockedStateChangedCallback>
      blocked_state_changed_callback;
  EXPECT_CALL(blocked_state_changed_callback, Run).Times(1);
  base::CallbackListSubscription subscription =
      tab_model->RegisterBlockedStateChanged(
          blocked_state_changed_callback.Get());
  tab_model->SetBlocked(true);
  testing::Mock::VerifyAndClearExpectations(&blocked_state_changed_callback);
}

TEST_F(TabModelTest, LoadIfNeeded) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  tabs::TabModel* const tab_model =
      static_cast<tabs::TabModel*>(tab_strip.GetTabAtIndex(0));

  content::WebContents* contents = tab_model->GetContents();
  EXPECT_FALSE(contents->GetController().NeedsReload());
  contents->GetController().SetNeedsReload();
  EXPECT_TRUE(contents->GetController().NeedsReload());
  tab_model->LoadIfNeeded();
  EXPECT_FALSE(contents->GetController().NeedsReload());
}

TEST_F(TabModelTest, Getters) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  tabs::TabModel* const tab_model =
      static_cast<tabs::TabModel*>(tab_strip.GetTabAtIndex(0));

  content::WebContents* contents = tab_model->GetContents();
  content::WebContentsTester* tester =
      content::WebContentsTester::For(contents);

  tester->SetTitle(u"Test Title");
  tester->NavigateAndCommit(GURL("https://example.com"));
  base::Time active_time = base::Time::FromSecondsSinceUnixEpoch(100);
  tester->SetLastActiveTime(active_time);

  EXPECT_EQ(u"Test Title", tab_model->GetTitle());
  EXPECT_EQ(GURL("https://example.com"), tab_model->GetURL());
  EXPECT_EQ(active_time, tab_model->GetLastActiveTime());
}

TEST_F(TabModelTest, SplitChangedCallback) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  AppendTab(tab_strip);
  AppendTab(tab_strip);
  AppendTab(tab_strip);
  tabs::TabModel* const tab_model0 =
      static_cast<tabs::TabModel*>(tab_strip.GetTabAtIndex(0));
  tabs::TabModel* const tab_model1 =
      static_cast<tabs::TabModel*>(tab_strip.GetTabAtIndex(1));

  // Activate tab 1 so AddToNewSplit({0, 1}) must propagate selection to tab 0.
  tab_strip.ActivateTabAt(1);

  SplitChangedCallbackMock split_changed_callback;
  base::CallbackListSubscription subscription0 =
      tab_model0->RegisterSplitChanged(split_changed_callback.Get());
  base::CallbackListSubscription subscription1 =
      tab_model1->RegisterSplitChanged(split_changed_callback.Get());

  // Unrelated group changes must not fire the split changed callback.
  tab_strip.AddToNewGroup({2});
  tab_strip.RemoveFromGroup({2});

  // Adding tabs to a split notifies only once the full split and selection
  // state are updated.
  std::optional<split_tabs::SplitTabId> notified_split0;
  std::optional<split_tabs::SplitTabId> notified_split1;
  auto verify_full_split = [&](std::optional<split_tabs::SplitTabId> split) {
    ASSERT_TRUE(split.has_value());
    EXPECT_TRUE(tab_strip.ContainsSplit(*split));
    EXPECT_EQ(2u, tab_strip.GetSplitData(*split)->ListTabs().size());
    EXPECT_TRUE(tab_model0->IsSplit());
    EXPECT_TRUE(tab_model1->IsSplit());
    EXPECT_TRUE(tab_model0->IsSelected());
    EXPECT_TRUE(tab_model1->IsSelected());
  };
  EXPECT_CALL(split_changed_callback, Run(tab_model0, testing::_))
      .WillOnce([&](tabs::TabInterface*,
                    std::optional<split_tabs::SplitTabId> split) {
        verify_full_split(split);
        notified_split0 = split;
      });
  EXPECT_CALL(split_changed_callback, Run(tab_model1, testing::_))
      .WillOnce([&](tabs::TabInterface*,
                    std::optional<split_tabs::SplitTabId> split) {
        verify_full_split(split);
        notified_split1 = split;
      });
  const split_tabs::SplitTabId split_id = tab_strip.AddToNewSplit(
      {0, 1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  testing::Mock::VerifyAndClearExpectations(&split_changed_callback);
  EXPECT_EQ(split_id, notified_split0);
  EXPECT_EQ(split_id, notified_split1);

  // Grouping or ungrouping the split tabs must not fire the split callback.
  tab_strip.AddToNewGroup({0, 1});
  tab_strip.RemoveFromGroup({0, 1});

  // Removing the split notifies only after the collection is removed and
  // inactive tab selection is cleared.
  auto verify_split_removed = [&](tabs::TabInterface*,
                                  std::optional<split_tabs::SplitTabId> split) {
    EXPECT_FALSE(split.has_value());
    EXPECT_FALSE(tab_strip.ContainsSplit(split_id));
    EXPECT_FALSE(tab_model0->IsSplit());
    EXPECT_FALSE(tab_model1->IsSplit());
    EXPECT_FALSE(tab_model0->IsSelected());
    EXPECT_TRUE(tab_model1->IsSelected());
  };
  EXPECT_CALL(split_changed_callback,
              Run(tab_model0, std::optional<split_tabs::SplitTabId>()))
      .WillOnce(verify_split_removed);
  EXPECT_CALL(split_changed_callback,
              Run(tab_model1, std::optional<split_tabs::SplitTabId>()))
      .WillOnce(verify_split_removed);
  tab_strip.RemoveSplit(split_id);
  testing::Mock::VerifyAndClearExpectations(&split_changed_callback);
}

TEST_F(TabModelTest, SplitChangedCallbackUpdateTabInSplit) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  for (int i = 0; i < 4; ++i) {
    AppendTab(tab_strip);
  }

  TabInterface* const surviving_tab = tab_strip.GetTabAtIndex(0);
  TabInterface* const first_partner = tab_strip.GetTabAtIndex(1);
  TabInterface* const swap_tab = tab_strip.GetTabAtIndex(2);
  TabInterface* const replace_tab = tab_strip.GetTabAtIndex(3);

  const split_tabs::SplitTabId split_id = tab_strip.AddToNewSplit(
      {0, 1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  SplitChangedCallbackMock callback;
  std::vector<base::CallbackListSubscription> subscriptions;
  for (TabInterface* tab :
       {surviving_tab, first_partner, swap_tab, replace_tab}) {
    subscriptions.push_back(tab->RegisterSplitChanged(callback.Get()));
  }

  auto verify_split_tabs = [&](TabInterface* first, TabInterface* second,
                               TabInterface* removed) {
    EXPECT_EQ(2u, tab_strip.GetSplitData(split_id)->ListTabs().size());
    EXPECT_EQ(split_id, first->GetSplit());
    EXPECT_EQ(split_id, second->GetSplit());
    EXPECT_EQ(std::nullopt, removed->GetSplit());
    EXPECT_TRUE(first->IsSelected());
    EXPECT_TRUE(second->IsSelected());
    EXPECT_FALSE(removed->IsSelected());
  };

  // Swapping a partner tab must not transiently fire unsplit on the surviving
  // tab, and callbacks must see the complete 2-tab split and selection state.
  tab_strip.ActivateTabAt(tab_strip.GetIndexOfTab(first_partner));
  EXPECT_CALL(callback,
              Run(first_partner, std::optional<split_tabs::SplitTabId>()))
      .WillOnce([&](TabInterface*, std::optional<split_tabs::SplitTabId>) {
        verify_split_tabs(surviving_tab, swap_tab, first_partner);
      });
  EXPECT_CALL(callback, Run(swap_tab, std::make_optional(split_id)))
      .WillOnce([&](TabInterface*, std::optional<split_tabs::SplitTabId>) {
        verify_split_tabs(surviving_tab, swap_tab, first_partner);
      });
  tab_strip.UpdateTabInSplit(first_partner->GetHandle(), swap_tab->GetHandle(),
                             TabStripModel::SplitUpdateType::kSwap);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Swapping the tab at index 0 of the split puts the incoming tab at index 0
  // during CreateSplit(); it must still wait until the full split is built.
  tab_strip.ActivateTabAt(tab_strip.GetIndexOfTab(surviving_tab));
  EXPECT_CALL(callback,
              Run(surviving_tab, std::optional<split_tabs::SplitTabId>()))
      .WillOnce([&](TabInterface*, std::optional<split_tabs::SplitTabId>) {
        verify_split_tabs(first_partner, swap_tab, surviving_tab);
      });
  EXPECT_CALL(callback, Run(first_partner, std::make_optional(split_id)))
      .WillOnce([&](TabInterface*, std::optional<split_tabs::SplitTabId>) {
        verify_split_tabs(first_partner, swap_tab, surviving_tab);
      });
  tab_strip.UpdateTabInSplit(surviving_tab->GetHandle(),
                             first_partner->GetHandle(),
                             TabStripModel::SplitUpdateType::kSwap);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Replacing a partner tab must not notify the surviving tab either.
  EXPECT_CALL(callback, Run(replace_tab, std::make_optional(split_id)))
      .WillOnce([&](TabInterface*, std::optional<split_tabs::SplitTabId>) {
        EXPECT_EQ(2u, tab_strip.GetSplitData(split_id)->ListTabs().size());
        EXPECT_EQ(split_id, first_partner->GetSplit());
        EXPECT_EQ(split_id, replace_tab->GetSplit());
        EXPECT_TRUE(first_partner->IsSelected());
        EXPECT_TRUE(replace_tab->IsSelected());
      });
  tab_strip.UpdateTabInSplit(swap_tab->GetHandle(), replace_tab->GetHandle(),
                             TabStripModel::SplitUpdateType::kReplace);
  testing::Mock::VerifyAndClearExpectations(&callback);
}

TEST_F(TabModelTest, SplitChangedCallbackCrossWindowDetach) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_src(&delegate, profile());
  TabStripModel tab_strip_dst(&delegate, profile());
  AppendTab(tab_strip_src);
  AppendTab(tab_strip_src);

  TabInterface* const tab0 = tab_strip_src.GetTabAtIndex(0);
  TabInterface* const tab1 = tab_strip_src.GetTabAtIndex(1);

  const split_tabs::SplitTabId split_id = tab_strip_src.AddToNewSplit(
      {0, 1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  SplitChangedCallbackMock callback;
  std::vector<base::CallbackListSubscription> subscriptions;
  subscriptions.push_back(tab0->RegisterSplitChanged(callback.Get()));
  subscriptions.push_back(tab1->RegisterSplitChanged(callback.Get()));

  auto detached_split = tab_strip_src.DetachSplitTabForInsertion(split_id);
  tab_strip_dst.InsertDetachedSplitTabAt(std::move(detached_split), 0, false);

  EXPECT_EQ(split_id, tab0->GetSplit());
  EXPECT_EQ(split_id, tab1->GetSplit());
}

}  // namespace tabs
