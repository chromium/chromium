// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_group.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/tabs/tab_group_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

namespace {

using ::testing::_;

class TabGroupTest : public ::testing::Test {
 public:
  TabGroupTest()
      : profile_(std::make_unique<TestingProfile>()),
        tab_strip_model_(
            std::make_unique<TabStripModel>(&delegate_, profile_.get())) {}
  TabGroupTest(const TabGroupTest&) = delete;
  TabGroupTest& operator=(const TabGroupTest&) = delete;
  ~TabGroupTest() override = default;

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  Profile* profile() { return profile_.get(); }

  // Creates a group collection that is not part of any tab strip. Its TabGroup
  // has the same bookkeeping as one owned by a TabStripModel, but no tab strip
  // indices.
  std::unique_ptr<tabs::TabGroupTabCollection> CreateDetachedGroupCollection(
      const tab_groups::TabGroupId& id,
      const tab_groups::TabGroupVisualData& visual_data) {
    TabGroupDesktop::Factory factory(profile());
    return std::make_unique<tabs::TabGroupTabCollection>(factory, id,
                                                         visual_data);
  }

  std::unique_ptr<tabs::TabModel> CreateTab() {
    return std::make_unique<tabs::TabModel>(
        content::WebContents::Create(
            content::WebContents::CreateParams(profile())),
        tab_strip_model());
  }

  void AppendTabs(int count) {
    for (int i = 0; i < count; ++i) {
      tab_strip_model()->AppendTab(CreateTab(), /*foreground=*/true);
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  std::unique_ptr<TestingProfile> profile_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(TabGroupTest, InitialState) {
  const tab_groups::TabGroupId id = tab_groups::TabGroupId::GenerateNew();
  const tab_groups::TabGroupVisualData visual_data(
      u"Title", tab_groups::TabGroupColorId::kBlue);
  std::unique_ptr<tabs::TabGroupTabCollection> collection =
      CreateDetachedGroupCollection(id, visual_data);
  const TabGroup* group = collection->GetTabGroup();

  EXPECT_EQ(id, group->id());
  EXPECT_EQ(visual_data, *group->visual_data());
  EXPECT_EQ(collection->GetHandle(), group->GetCollectionHandle());
  EXPECT_EQ(0, group->tab_count());
  EXPECT_TRUE(group->IsEmpty());
  EXPECT_FALSE(group->IsCustomized());
  EXPECT_EQ(nullptr, group->GetFirstTab());
  EXPECT_EQ(nullptr, group->GetLastTab());
  EXPECT_EQ(gfx::Range(), group->ListTabs());
}

TEST_F(TabGroupTest, SetVisualDataNotifiesAndReplacesData) {
  std::unique_ptr<tabs::TabGroupTabCollection> collection =
      CreateDetachedGroupCollection(tab_groups::TabGroupId::GenerateNew(),
                                    tab_groups::TabGroupVisualData());
  TabGroup* group = collection->GetTabGroup();

  base::MockCallback<base::RepeatingClosure> visual_data_changed;
  base::MockCallback<base::RepeatingClosure> group_changed;
  base::CallbackListSubscription visual_data_subscription =
      group->RegisterOnVisualDataChanged(visual_data_changed.Get());
  base::CallbackListSubscription group_changed_subscription =
      group->RegisterOnGroupChanged(group_changed.Get());

  const tab_groups::TabGroupVisualData new_data(
      u"Renamed", tab_groups::TabGroupColorId::kRed, /*is_collapsed=*/true);
  EXPECT_CALL(visual_data_changed, Run()).Times(1);
  EXPECT_CALL(group_changed, Run()).Times(0);
  group->SetVisualData(new_data);
  EXPECT_EQ(new_data, *group->visual_data());

  // Dropping the subscription stops the notifications.
  visual_data_subscription = {};
  EXPECT_CALL(visual_data_changed, Run()).Times(0);
  group->SetVisualData(tab_groups::TabGroupVisualData());
}

TEST_F(TabGroupTest, IsCustomizedIsSticky) {
  std::unique_ptr<tabs::TabGroupTabCollection> collection =
      CreateDetachedGroupCollection(tab_groups::TabGroupId::GenerateNew(),
                                    tab_groups::TabGroupVisualData());
  TabGroup* group = collection->GetTabGroup();
  const tab_groups::TabGroupVisualData data(u"Title",
                                            tab_groups::TabGroupColorId::kGrey);

  // Data set on the group's behalf (e.g. by session restore) does not count
  // as a customization.
  group->SetVisualData(data);
  EXPECT_FALSE(group->IsCustomized());

  group->SetVisualData(data, /*is_customized=*/true);
  EXPECT_TRUE(group->IsCustomized());

  // Once customized, later non-customizing updates keep the flag.
  group->SetVisualData(tab_groups::TabGroupVisualData());
  EXPECT_TRUE(group->IsCustomized());
}

TEST_F(TabGroupTest, TabCountBookkeepingNotifiesGroupChanged) {
  std::unique_ptr<tabs::TabGroupTabCollection> collection =
      CreateDetachedGroupCollection(tab_groups::TabGroupId::GenerateNew(),
                                    tab_groups::TabGroupVisualData());
  TabGroup* group = collection->GetTabGroup();

  base::MockCallback<base::RepeatingClosure> group_changed;
  base::MockCallback<base::RepeatingClosure> visual_data_changed;
  base::CallbackListSubscription group_changed_subscription =
      group->RegisterOnGroupChanged(group_changed.Get());
  base::CallbackListSubscription visual_data_subscription =
      group->RegisterOnVisualDataChanged(visual_data_changed.Get());
  EXPECT_CALL(visual_data_changed, Run()).Times(0);

  EXPECT_CALL(group_changed, Run()).Times(2);
  group->AddTab();
  group->AddTab();
  EXPECT_EQ(2, group->tab_count());
  EXPECT_FALSE(group->IsEmpty());

  // A move changes nothing about the count but still notifies.
  EXPECT_CALL(group_changed, Run()).Times(1);
  group->MoveTab();
  EXPECT_EQ(2, group->tab_count());

  EXPECT_CALL(group_changed, Run()).Times(2);
  group->RemoveTab();
  group->RemoveTab();
  EXPECT_EQ(0, group->tab_count());
  EXPECT_TRUE(group->IsEmpty());

  group_changed_subscription = {};
  EXPECT_CALL(group_changed, Run()).Times(0);
  group->AddTab();
}

TEST_F(TabGroupTest, SetGroupIsClosing) {
  std::unique_ptr<tabs::TabGroupTabCollection> collection =
      CreateDetachedGroupCollection(tab_groups::TabGroupId::GenerateNew(),
                                    tab_groups::TabGroupVisualData());
  TabGroup* group = collection->GetTabGroup();

  EXPECT_FALSE(group->IsGroupClosing());
  group->SetGroupIsClosing(true);
  EXPECT_TRUE(group->IsGroupClosing());
  group->SetGroupIsClosing(false);
  EXPECT_FALSE(group->IsGroupClosing());
}

TEST_F(TabGroupTest, FirstLastAndListTabsFollowTheTabStrip) {
  AppendTabs(4);
  const tab_groups::TabGroupId id = tab_strip_model()->AddToNewGroup({1, 2});
  const TabGroup* group = tab_strip_model()->group_model()->GetTabGroup(id);
  ASSERT_TRUE(group);

  EXPECT_EQ(2, group->tab_count());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(1), group->GetFirstTab());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(2), group->GetLastTab());
  EXPECT_EQ(gfx::Range(1, 3), group->ListTabs());

  // Growing the group at the end moves the last tab and the range.
  tab_strip_model()->AddToExistingGroup({3}, id);
  EXPECT_EQ(3, group->tab_count());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(1), group->GetFirstTab());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(3), group->GetLastTab());
  EXPECT_EQ(gfx::Range(1, 4), group->ListTabs());

  // Moving the whole group shifts the range without changing its size.
  tab_strip_model()->MoveGroupTo(id, 0);
  EXPECT_EQ(3, group->tab_count());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(0), group->GetFirstTab());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(2), group->GetLastTab());
  EXPECT_EQ(gfx::Range(0, 3), group->ListTabs());

  // Removing the first tab of the group moves the first tab.
  tab_strip_model()->RemoveFromGroup({0});
  EXPECT_EQ(2, group->tab_count());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(1), group->GetFirstTab());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(2), group->GetLastTab());
  EXPECT_EQ(gfx::Range(1, 3), group->ListTabs());
}

TEST_F(TabGroupTest, SingleTabGroup) {
  AppendTabs(2);
  const tab_groups::TabGroupId id = tab_strip_model()->AddToNewGroup({1});
  const TabGroup* group = tab_strip_model()->group_model()->GetTabGroup(id);
  ASSERT_TRUE(group);

  EXPECT_EQ(1, group->tab_count());
  EXPECT_EQ(group->GetFirstTab(), group->GetLastTab());
  EXPECT_EQ(tab_strip_model()->GetTabAtIndex(1), group->GetFirstTab());
  EXPECT_EQ(gfx::Range(1, 2), group->ListTabs());
}

TEST_F(TabGroupTest, TabsAreNotIndexableOutsideATabStrip) {
  // A group collection that hangs off no tab strip has tabs, but no tab strip
  // indices to report for them.
  std::unique_ptr<tabs::TabGroupTabCollection> collection =
      CreateDetachedGroupCollection(tab_groups::TabGroupId::GenerateNew(),
                                    tab_groups::TabGroupVisualData());
  collection->AddTab(CreateTab(), 0);
  collection->AddTab(CreateTab(), 1);
  const TabGroup* group = collection->GetTabGroup();

  EXPECT_EQ(nullptr, group->GetFirstTab());
  EXPECT_EQ(nullptr, group->GetLastTab());
  EXPECT_EQ(gfx::Range(), group->ListTabs());
}

TEST_F(TabGroupTest, WeakPtrIsInvalidatedWithTheGroup) {
  std::unique_ptr<tabs::TabGroupTabCollection> collection =
      CreateDetachedGroupCollection(tab_groups::TabGroupId::GenerateNew(),
                                    tab_groups::TabGroupVisualData());
  base::WeakPtr<TabGroup> weak_group = collection->GetTabGroup()->AsWeakPtr();
  base::WeakPtr<const TabGroup> const_weak_group =
      std::as_const(*collection->GetTabGroup()).AsWeakPtr();
  EXPECT_TRUE(weak_group);
  EXPECT_TRUE(const_weak_group);
  EXPECT_EQ(collection->GetTabGroup(), weak_group.get());

  collection.reset();
  EXPECT_FALSE(weak_group);
  EXPECT_FALSE(const_weak_group);
}

}  // namespace
