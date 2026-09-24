// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace {

class AppMenuSearchControllerTest : public testing::Test {
 public:
  void SetUp() override {
    scope_ = actions::ActionItem::Builder().Build();
    menu_root_ = actions::ActionItem::Builder().Build();
    controller_ = std::make_unique<AppMenuSearchController>(menu_root_.get());
  }

 protected:
  // `scope_` must be declared before `menu_root_` so that underlying delegate
  // ActionItems outlive any IndirectActionItems stored in `menu_root_`.
  std::unique_ptr<actions::ActionItem> scope_;
  std::unique_ptr<actions::ActionItem> menu_root_;
  std::unique_ptr<AppMenuSearchController> controller_;
};

TEST_F(AppMenuSearchControllerTest, InitializeSearchIndexOnlyOnce) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());

  EXPECT_FALSE(controller_->is_search_index_initialized_for_testing());

  controller_->InitializeSearchIndex();
  EXPECT_TRUE(controller_->is_search_index_initialized_for_testing());
  EXPECT_EQ(controller_->search_items_for_testing().size(), 1u);

  EXPECT_CHECK_DEATH(controller_->InitializeSearchIndex());
}

TEST_F(AppMenuSearchControllerTest, FlattenHierarchyFiltering) {
  auto section = actions::ActionItem::Builder().Build();
  section->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                       AppMenuActionItem::DisplayType::kSection);
  section->AddChild(AppMenuActionItem::CreateHeader(u"Main Section"));
  section->AddChild(actions::ActionItem::Builder()
                        .SetActionId(kActionNewTab)
                        .SetText(u"New Tab")
                        .SetVisible(true)
                        .SetEnabled(true)
                        .Build());

  section->AddChild(actions::ActionItem::Builder()
                        .SetActionId(kActionNewIncognitoWindow)
                        .SetText(u"New Incognito Window")
                        .SetVisible(true)
                        .SetEnabled(false)
                        .Build());

  section->AddChild(actions::ActionItem::Builder()
                        .SetActionId(kActionRestoreTab)
                        .SetText(u"Restore Tab")
                        .SetVisible(false)
                        .SetEnabled(true)
                        .Build());

  menu_root_->AddChild(std::move(section));

  auto divider = actions::ActionItem::Builder().Build();
  divider->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                       AppMenuActionItem::DisplayType::kDivider);
  menu_root_->AddChild(std::move(divider));

  auto search_row = actions::ActionItem::Builder().Build();
  search_row->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                          AppMenuActionItem::DisplayType::kSearch);
  menu_root_->AddChild(std::move(search_row));

  auto footer = actions::ActionItem::Builder().Build();
  footer->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                      AppMenuActionItem::DisplayType::kFooter);
  menu_root_->AddChild(std::move(footer));

  auto override_item = actions::ActionItem::Builder()
                           .SetActionId(kActionPrint)
                           .SetText(u"Original Text")
                           .SetVisible(true)
                           .SetEnabled(true)
                           .Build();
  override_item->SetProperty(AppMenuActionItem::kTextOverrideKey,
                             u"Overridden Text");
  menu_root_->AddChild(std::move(override_item));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0]->GetTitle(), u"New Tab");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kAction);

  EXPECT_EQ(items[1]->GetTitle(), u"Overridden Text");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kAction);
}

TEST_F(AppMenuSearchControllerTest,
       FlattenHierarchyIndexesChildrenOfUntitledRowContainer) {
  auto row = actions::ActionItem::Builder().Build();
  row->AddChild(actions::ActionItem::Builder()
                    .SetActionId(kActionCut)
                    .SetText(u"Cut")
                    .Build());
  row->AddChild(actions::ActionItem::Builder()
                    .SetActionId(kActionCopy)
                    .SetText(u"Copy")
                    .Build());
  row->AddChild(actions::ActionItem::Builder()
                    .SetActionId(kActionPaste)
                    .SetText(u"Paste")
                    .Build());
  menu_root_->AddChild(std::move(row));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 3u);

  EXPECT_EQ(items[0]->GetTitle(), u"Cut");
  EXPECT_EQ(items[0]->GetSecondaryText(), u"");

  EXPECT_EQ(items[1]->GetTitle(), u"Copy");
  EXPECT_EQ(items[1]->GetSecondaryText(), u"");

  EXPECT_EQ(items[2]->GetTitle(), u"Paste");
  EXPECT_EQ(items[2]->GetSecondaryText(), u"");
}

TEST_F(AppMenuSearchControllerTest, SubmenusAndBreadcrumbs) {
  auto bookmarks_submenu = actions::ActionItem::Builder()
                               .SetActionId(kActionBookmarksSubmenu)
                               .SetText(u"Bookmarks and lists")
                               .Build();

  bookmarks_submenu->AddChild(actions::ActionItem::Builder()
                                  .SetActionId(kActionBookmarkThisTab)
                                  .SetText(u"Bookmark this tab")
                                  .Build());

  auto nested_folder = actions::ActionItem::Builder().SetText(u"Work").Build();
  nested_folder->AddChild(
      actions::ActionItem::Builder().SetText(u"Google Doc").Build());
  bookmarks_submenu->AddChild(std::move(nested_folder));

  menu_root_->AddChild(std::move(bookmarks_submenu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0]->GetTitle(), u"Bookmark this tab");
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Bookmarks and lists");

  EXPECT_EQ(items[1]->GetTitle(), u"Google Doc");
  EXPECT_EQ(items[1]->GetSecondaryText(), u"Work");
}

TEST_F(AppMenuSearchControllerTest, ZoomSubmenuAndTooltips) {
  auto zoom_submenu = actions::ActionItem::Builder()
                          .SetActionId(kActionZoomSubmenu)
                          .SetText(u"Zoom")
                          .Build();

  zoom_submenu->AddChild(actions::ActionItem::Builder()
                             .SetActionId(kActionZoomMinus)
                             .SetTooltipText(u"Zoom Out")
                             .Build());

  zoom_submenu->AddChild(actions::ActionItem::Builder()
                             .SetActionId(kActionZoomPlus)
                             .SetTooltipText(u"Zoom In")
                             .Build());

  zoom_submenu->AddChild(actions::ActionItem::Builder()
                             .SetActionId(kActionFullscreen)
                             .SetTooltipText(u"Fullscreen")
                             .Build());

  menu_root_->AddChild(std::move(zoom_submenu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 3u);

  EXPECT_EQ(items[0]->GetTitle(), u"Zoom Out");
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Zoom");
  EXPECT_EQ(items[0]->GetActionItem()->GetActionId(), kActionZoomMinus);

  EXPECT_EQ(items[1]->GetTitle(), u"Zoom In");
  EXPECT_EQ(items[1]->GetSecondaryText(), u"Zoom");
  EXPECT_EQ(items[1]->GetActionItem()->GetActionId(), kActionZoomPlus);

  EXPECT_EQ(items[2]->GetTitle(), u"Fullscreen");
  EXPECT_EQ(items[2]->GetSecondaryText(), u"Zoom");
  EXPECT_EQ(items[2]->GetActionItem()->GetActionId(), kActionFullscreen);
}

TEST_F(AppMenuSearchControllerTest,
       GetSearchItemTypeForBookmarksAndRecentTabs) {
  auto bookmarks_menu = actions::ActionItem::Builder()
                            .SetActionId(kActionBookmarksSubmenu)
                            .SetText(u"Bookmarks")
                            .Build();

  bookmarks_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"Bookmark Manager").Build());

  auto folder = actions::ActionItem::Builder().SetText(u"Work").Build();
  folder->AddChild(
      actions::ActionItem::Builder().SetText(u"Google Doc").Build());
  bookmarks_menu->AddChild(std::move(folder));

  menu_root_->AddChild(std::move(bookmarks_menu));

  auto recent_tabs_menu = actions::ActionItem::Builder()
                              .SetActionId(kActionRecentTabsSubmenu)
                              .SetText(u"History")
                              .Build();

  recent_tabs_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"Recently Closed Tab").Build());

  auto remote_device =
      actions::ActionItem::Builder().SetText(u"Edward's Android").Build();
  remote_device->AddChild(
      actions::ActionItem::Builder().SetText(u"Remote Article").Build());
  recent_tabs_menu->AddChild(std::move(remote_device));

  menu_root_->AddChild(std::move(recent_tabs_menu));

  auto tab_groups_menu = actions::ActionItem::Builder()
                             .SetActionId(kActionSavedTabGroupsSubmenu)
                             .SetText(u"Tab Groups")
                             .Build();

  tab_groups_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"Research Group").Build());

  menu_root_->AddChild(std::move(tab_groups_menu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 5u);

  EXPECT_EQ(items[0]->GetTitle(), u"Bookmark Manager");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Bookmarks");

  EXPECT_EQ(items[1]->GetTitle(), u"Google Doc");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(items[1]->GetSecondaryText(), u"Work");

  EXPECT_EQ(items[2]->GetTitle(), u"Recently Closed Tab");
  EXPECT_EQ(items[2]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[2]->GetSecondaryText(), u"History");

  EXPECT_EQ(items[3]->GetTitle(), u"Remote Article");
  EXPECT_EQ(items[3]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[3]->GetSecondaryText(), u"Edward's Android");

  EXPECT_EQ(items[4]->GetTitle(), u"Research Group");
  EXPECT_EQ(items[4]->GetType(), AppMenuSearchItem::Type::kTabGroup);
  EXPECT_EQ(items[4]->GetSecondaryText(), u"Tab Groups");
}

TEST_F(AppMenuSearchControllerTest, StaticActionsAndTabGroups) {
  auto bookmarks_menu = actions::ActionItem::Builder()
                            .SetActionId(kActionBookmarksSubmenu)
                            .SetText(u"Bookmarks")
                            .Build();
  bookmarks_menu->AddChild(actions::ActionItem::Builder()
                               .SetActionId(kActionBookmarkThisTab)
                               .SetText(u"Bookmark this tab")
                               .Build());
  bookmarks_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"Amazon").Build());
  menu_root_->AddChild(std::move(bookmarks_menu));

  auto tab_groups_menu = actions::ActionItem::Builder()
                             .SetActionId(kActionSavedTabGroupsSubmenu)
                             .SetText(u"Tab Groups")
                             .Build();
  tab_groups_menu->AddChild(actions::ActionItem::Builder()
                                .SetActionId(kActionCreateNewTabGroup)
                                .SetText(u"Create new tab group")
                                .Build());

  auto group_entity = actions::ActionItem::Builder().SetText(u"Work").Build();
  group_entity->AddChild(actions::ActionItem::Builder()
                             .SetActionId(kActionTabGroupOpenInBrowser)
                             .SetText(u"Open group")
                             .Build());
  group_entity->AddChild(actions::ActionItem::Builder()
                             .SetActionId(kActionTabGroupDelete)
                             .SetText(u"Delete group")
                             .Build());
  tab_groups_menu->AddChild(std::move(group_entity));
  menu_root_->AddChild(std::move(tab_groups_menu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 5u);

  EXPECT_EQ(items[0]->GetTitle(), u"Bookmark this tab");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Bookmarks");

  EXPECT_EQ(items[1]->GetTitle(), u"Amazon");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(items[1]->GetSecondaryText(), u"Bookmarks");

  EXPECT_EQ(items[2]->GetTitle(), u"Create new tab group");
  EXPECT_EQ(items[2]->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(items[2]->GetSecondaryText(), u"Tab Groups");

  // Tab group child actions are traversed and extracted as actionable items.
  EXPECT_EQ(items[3]->GetTitle(), u"Open group");
  EXPECT_EQ(items[3]->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(items[3]->GetSecondaryText(), u"Work");
  EXPECT_EQ(items[3]->GetActionItem()->GetActionId(),
            kActionTabGroupOpenInBrowser);

  EXPECT_EQ(items[4]->GetTitle(), u"Delete group");
  EXPECT_EQ(items[4]->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(items[4]->GetSecondaryText(), u"Work");
  EXPECT_EQ(items[4]->GetActionItem()->GetActionId(), kActionTabGroupDelete);
}

TEST_F(AppMenuSearchControllerTest, FlattenHierarchyExtractsSynonyms) {
  std::vector<std::u16string> synonyms = {u"private", u"secret"};
  auto action_item = actions::ActionItem::Builder()
                         .SetActionId(kActionNewIncognitoWindow)
                         .SetText(u"New Incognito Window")
                         .AddSynonyms({u"private", u"secret"})
                         .Build();

  menu_root_->AddChild(std::move(action_item));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 1u);

  EXPECT_EQ(items[0]->GetTitle(), u"New Incognito Window");
  EXPECT_EQ(items[0]->GetSynonyms(), synonyms);
}

TEST_F(AppMenuSearchControllerTest, FlattenHierarchyRecentTabsSubmenu) {
  auto recent_tabs_menu = actions::ActionItem::Builder()
                              .SetActionId(kActionRecentTabsSubmenu)
                              .SetText(u"History")
                              .Build();

  // 1. System action:
  recent_tabs_menu->AddChild(actions::ActionItem::Builder()
                                 .SetActionId(kActionShowHistory)
                                 .SetText(u"Open history page")
                                 .Build());

  // 2. Section header (should be pruned):
  recent_tabs_menu->AddChild(
      AppMenuActionItem::CreateHeader(u"Recently closed"));

  // 3. Local single closed tab:
  recent_tabs_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"GitHub").Build());

  // 4. Local multi-tab window container ("3 tabs"):
  auto window_container =
      actions::ActionItem::Builder().SetText(u"3 tabs").Build();
  window_container->AddChild(
      actions::ActionItem::Builder().SetText(u"Restore window").Build());
  auto window_divider = actions::ActionItem::Builder().Build();
  window_divider->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                              AppMenuActionItem::DisplayType::kDivider);
  window_container->AddChild(std::move(window_divider));
  window_container->AddChild(
      actions::ActionItem::Builder().SetText(u"Window Tab 1").Build());
  window_container->AddChild(
      actions::ActionItem::Builder().SetText(u"Window Tab 2").Build());
  recent_tabs_menu->AddChild(std::move(window_container));

  // 5. Remote synced device container ("Edward's Android"):
  auto device_container =
      actions::ActionItem::Builder().SetText(u"Edward's Android").Build();
  device_container->AddChild(
      actions::ActionItem::Builder().SetText(u"Phone Tab 1").Build());
  device_container->AddChild(
      actions::ActionItem::Builder().SetText(u"Phone Tab 2").Build());
  recent_tabs_menu->AddChild(std::move(device_container));

  menu_root_->AddChild(std::move(recent_tabs_menu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 7u);

  // System action:
  EXPECT_EQ(items[0]->GetTitle(), u"Open history page");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"History");

  // Local single tab:
  EXPECT_EQ(items[1]->GetTitle(), u"GitHub");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[1]->GetSecondaryText(), u"History");

  // Local window tabs (divider skipped):
  EXPECT_EQ(items[2]->GetTitle(), u"Restore window");
  EXPECT_EQ(items[2]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[2]->GetSecondaryText(), u"3 tabs");

  EXPECT_EQ(items[3]->GetTitle(), u"Window Tab 1");
  EXPECT_EQ(items[3]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[3]->GetSecondaryText(), u"3 tabs");

  EXPECT_EQ(items[4]->GetTitle(), u"Window Tab 2");
  EXPECT_EQ(items[4]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[4]->GetSecondaryText(), u"3 tabs");

  // Remote device tabs (device name as secondary text):
  EXPECT_EQ(items[5]->GetTitle(), u"Phone Tab 1");
  EXPECT_EQ(items[5]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[5]->GetSecondaryText(), u"Edward's Android");

  EXPECT_EQ(items[6]->GetTitle(), u"Phone Tab 2");
  EXPECT_EQ(items[6]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[6]->GetSecondaryText(), u"Edward's Android");
}

TEST_F(AppMenuSearchControllerTest, EmptySubmenusAreNotIndexed) {
  // Dynamic submenu created via IndirectActionItem (matching
  // ActionAppMenuManager::AddDynamicSubmenu) with a populate callback on the
  // wrapper that produces zero children should not be indexed as a leaf.
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionSavedTabGroupsSubmenu)
                       .SetText(u"Tab groups")
                       .Build());
  auto indirect_submenu = AppMenuActionItem::CreateIndirect(
      kActionSavedTabGroupsSubmenu, scope_.get(),
      {.display_type = AppMenuActionItem::DisplayType::kRow});
  indirect_submenu->SetPopulateChildrenCallback(
      base::BindRepeating([](actions::BaseAction* parent) {}));
  menu_root_->AddChild(std::move(indirect_submenu));

  // Dynamic submenu with a populate callback directly on the ActionItem that
  // produces zero children should not be indexed as a leaf.
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetText(u"Dynamic Folder")
                           .SetPopulateChildrenCallback(base::BindRepeating(
                               [](actions::BaseAction* parent) {}))
                           .Build());

  controller_->InitializeSearchIndex();

  EXPECT_TRUE(controller_->search_items_for_testing().empty());
}

TEST_F(AppMenuSearchControllerTest,
       IndirectActionItemsWithTextOverrideAndDisplayType) {
  // Populate `scope_` with the underlying delegate ActionItems for
  // CreateIndirect().
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionBookmarksSubmenu)
                       .SetText(u"Default Bookmarks Title")
                       .Build());
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionShowBookmarkBar)
                       .SetText(u"Show bookmark bar")
                       .Build());
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionNewTab)
                       .SetText(u"Search Input")
                       .Build());

  // Build submenu via CreateIndirect() with a text override on the wrapper.
  auto bookmarks_submenu = AppMenuActionItem::CreateIndirect(
      kActionBookmarksSubmenu, scope_.get(),
      {
          .display_type = AppMenuActionItem::DisplayType::kRow,
          .text_override = u"Bookmarks and lists",
      });
  ASSERT_TRUE(bookmarks_submenu);

  // Add a normal row child with a text override on its IndirectActionItem.
  bookmarks_submenu->AddChild(AppMenuActionItem::CreateIndirect(
      kActionShowBookmarkBar, scope_.get(),
      {
          .display_type = AppMenuActionItem::DisplayType::kRow,
          .text_override = u"Hide bookmark bar",
      }));

  // Add a non-actionable child (kSearch DisplayType on the delegate ActionItem)
  // that should be skipped by CanProcessItem().
  bookmarks_submenu->AddChild(AppMenuActionItem::CreateIndirect(
      kActionNewTab, scope_.get(),
      {
          .display_type = AppMenuActionItem::DisplayType::kSearch,
      }));

  menu_root_->AddChild(std::move(bookmarks_submenu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 1u);

  // Verifies kTextOverrideKey was read from the IndirectActionItem wrapper for
  // both the leaf title and parent submenu breadcrumb, while kDisplayTypeKey on
  // the delegate skipped the search item.
  EXPECT_EQ(items[0]->GetTitle(), u"Hide bookmark bar");
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Bookmarks and lists");
}

}  // namespace
