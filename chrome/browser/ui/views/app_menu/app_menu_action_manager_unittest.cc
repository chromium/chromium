// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_action_manager.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/app_menu_proxy_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

using ::testing::_;

class AppMenuActionManagerTest : public testing::Test {
 public:
  AppMenuActionManagerTest() = default;
  ~AppMenuActionManagerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    actions::ActionManager::Get().ResetActions();

    // Register our test delegate actions in a global root container.
    auto root = actions::ActionItem::Builder().Build();
    root->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                kActionShowDownloads))
            .SetActionId(kActionShowDownloads)
            .SetText(u"Downloads")
            .SetEnabled(true)
            .SetVisible(true)
            .Build());
    root->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                kActionClearBrowsingData))
            .SetActionId(kActionClearBrowsingData)
            .SetText(u"Clear browsing data")
            .SetEnabled(true)
            .SetVisible(true)
            .Build());

    actions::ActionManager::Get().AddAction(std::move(root));
  }

  void TearDown() override {
    actions::ActionManager::Get().ResetActions();
    testing::Test::TearDown();
  }

 protected:
  using MockActionCallback =
      testing::MockFunction<void(actions::ActionId,
                                 actions::ActionItem*,
                                 actions::ActionInvocationContext)>;
  MockActionCallback mock_action_invoked_;
};

TEST_F(AppMenuActionManagerTest, InitializeInflatesHierarchy) {
  AppMenuActionManager manager;
  manager.Initialize();

  actions::ActionItem* root = manager.root_action_item();
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->GetChildren().children().size(), 1u);

  actions::ActionItem* section = root->GetChildren().children()[0].get();
  EXPECT_TRUE(actions::IsActionItemClass<AppMenuSectionActionItem>(section));
  EXPECT_EQ(section->GetText(), u"Your Chrome");
  EXPECT_EQ(section->GetProperty(AppMenuActionManager::kAppMenuDisplayTypeKey),
            MenuEntry::DisplayType::kRow);

  const auto& section_children = section->GetChildren().children();
  ASSERT_EQ(section_children.size(), 2u);

  actions::ActionItem* downloads_proxy = section_children[0].get();
  EXPECT_TRUE(
      actions::IsActionItemClass<AppMenuProxyActionItem>(downloads_proxy));
  EXPECT_EQ(downloads_proxy->GetActionId(), kActionShowDownloads);
  EXPECT_EQ(downloads_proxy->GetText(), u"Downloads");

  actions::ActionItem* clear_data_proxy = section_children[1].get();
  EXPECT_TRUE(
      actions::IsActionItemClass<AppMenuProxyActionItem>(clear_data_proxy));
  EXPECT_EQ(clear_data_proxy->GetActionId(), kActionClearBrowsingData);
  EXPECT_EQ(clear_data_proxy->GetText(), u"Clear browsing data");
}

TEST_F(AppMenuActionManagerTest, ProxySyncsWithDelegateAndInvokes) {
  AppMenuActionManager manager;
  manager.Initialize();

  actions::ActionItem* delegate =
      actions::ActionManager::Get().FindAction(kActionShowDownloads);
  ASSERT_NE(delegate, nullptr);

  actions::ActionItem* root = manager.root_action_item();
  actions::ActionItem* downloads_proxy =
      root->GetChildren().children()[0]->GetChildren().children()[0].get();

  // Test dynamic synchronization.
  EXPECT_EQ(downloads_proxy->GetText(), u"Downloads");
  delegate->SetText(u"New Downloads Title");
  EXPECT_EQ(downloads_proxy->GetText(), u"New Downloads Title");

  EXPECT_TRUE(downloads_proxy->GetEnabled());
  delegate->SetEnabled(false);
  EXPECT_FALSE(downloads_proxy->GetEnabled());

  // Re-enable the delegate so that InvokeAction() can execute the callback.
  delegate->SetEnabled(true);
  EXPECT_TRUE(downloads_proxy->GetEnabled());

  EXPECT_CALL(mock_action_invoked_, Call(kActionShowDownloads, _, _)).Times(1);
  downloads_proxy->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);
}
