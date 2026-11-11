// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_item.h"
#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"
#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process_death_test_mixin.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

namespace {

class MockDelegate : public AppMenuSearchController::Delegate {
 public:
  MOCK_METHOD(void,
              ShowSearchResults,
              (actions::ActionItem*, std::unique_ptr<actions::ActionItem>),
              (override));
  MOCK_METHOD(void,
              RestoreMenu,
              (std::unique_ptr<actions::ActionItem>),
              (override));
};

class AppMenuSearchControllerTest
    : public chrome_test_utils::TestingBrowserProcessDeathTestMixin,
      public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    scope_ = actions::ActionItem::Builder().Build();
    menu_root_ = actions::ActionItem::Builder().Build();
    search_bar_ = std::make_unique<AppMenuSearchBarView>();
    controller_ = std::make_unique<AppMenuSearchController>(
        menu_root_.get(), search_bar_.get(), &delegate_);
  }

  void TearDown() override {
    controller_.reset();
    search_bar_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  // `scope_` and `recent_tabs_menu_` must be declared before `menu_root_` so
  // that underlying delegate ActionItems and the recent tab actions bound to
  // `recent_tabs_menu_` outlive the actions stored in `menu_root_`.
  std::unique_ptr<actions::ActionItem> scope_;
  testing::NiceMock<MockBrowserWindowInterface> mock_window_;
  testing::NiceMock<MockAppMenuDragAndDropHost> mock_host_;
  RecentTabsDynamicMenu recent_tabs_menu_{&mock_window_, &mock_host_};
  std::unique_ptr<actions::ActionItem> menu_root_;
  std::unique_ptr<AppMenuSearchBarView> search_bar_;
  // A StrictMock, so tests fail on any delegate call they don't expect.
  testing::StrictMock<MockDelegate> delegate_;
  std::unique_ptr<AppMenuSearchController> controller_;
};

RecentTabItem MakeRecentTab(std::u16string title, std::string_view url) {
  RecentTabItem tab(RecentTabItem::Type::kTab, std::move(title));
  tab.set_url(GURL(url));
  return tab;
}

TEST_F(AppMenuSearchControllerTest, InitializeSearchIndexOnlyOnce) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());

  EXPECT_FALSE(controller_->is_search_index_initialized_for_testing());

  controller_->InitializeSearchIndex();
  EXPECT_TRUE(controller_->is_search_index_initialized_for_testing());
  EXPECT_EQ(controller_->search_items_for_testing().size(), 1u);

  // TODO(crbug.com/41487832): Enable on ChromeOS when test setup in the death
  // subprocess is fixed. AshTestHelper causes ICU/Timezone crashes in the
  // forked process before the CHECK can fire.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_CHECK_DEATH(controller_->InitializeSearchIndex());
#endif
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

  // The footer container is not a result itself, but its actions are indexed.
  auto footer = actions::ActionItem::Builder().Build();
  footer->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                      AppMenuActionItem::DisplayType::kFooter);
  footer->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionOptions)
                       .SetText(u"Settings")
                       .Build());
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
  ASSERT_EQ(items.size(), 3u);

  EXPECT_EQ(items[0]->GetTitle(), u"New Tab");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kAction);

  EXPECT_EQ(items[1]->GetTitle(), u"Settings");
  EXPECT_EQ(items[1]->GetSecondaryText(), u"");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kAction);

  EXPECT_EQ(items[2]->GetTitle(), u"Overridden Text");
  EXPECT_EQ(items[2]->GetType(), AppMenuSearchItem::Type::kAction);
}

TEST_F(AppMenuSearchControllerTest, FlattenHierarchyExcludesNotifications) {
  auto notification_section = actions::ActionItem::Builder().Build();
  notification_section->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                                    AppMenuActionItem::DisplayType::kSection);
  auto notification = actions::ActionItem::Builder()
                          .SetActionId(kActionUpgradeDialog)
                          .SetText(u"Update Chrome")
                          .SetVisible(true)
                          .SetEnabled(true)
                          .Build();
  notification->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                            AppMenuActionItem::DisplayType::kNotification);
  notification_section->AddChild(std::move(notification));
  menu_root_->AddChild(std::move(notification_section));

  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .SetVisible(true)
                           .SetEnabled(true)
                           .Build());

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0]->GetTitle(), u"New Tab");
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

TEST_F(AppMenuSearchControllerTest,
       ExtractsDomainFromBookmarkUrlAsSecondaryText) {
  TestingProfile profile;
  BookmarkModelFactory::GetInstance()->SetTestingFactory(
      &profile, BookmarkModelFactory::GetDefaultFactory());
  BookmarkMergedSurfaceServiceFactory::GetInstance()->SetTestingFactory(
      &profile, BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(&profile);
  ASSERT_TRUE(model);
  model->LoadEmptyForTest();

  BookmarkMergedSurfaceService* service =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);
  service->LoadForTesting({});

  const bookmarks::BookmarkNode* work_folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Work");
  model->AddURL(work_folder, 0, u"Google Doc",
                GURL("https://www.google.com/docs/document/d/123"));
  model->AddFolder(model->bookmark_bar_node(), 1, u"Empty Folder");

  testing::NiceMock<MockBrowserWindowInterface> mock_window;
  ON_CALL(mock_window, GetProfile()).WillByDefault(testing::Return(&profile));
  testing::NiceMock<MockAppMenuDragAndDropHost> mock_host;

  BookmarksDynamicMenu bookmarks_menu(&mock_window, &mock_host);
  auto bookmarks_submenu = actions::ActionItem::Builder()
                               .SetActionId(kActionBookmarksSubmenu)
                               .SetText(u"Bookmarks and lists")
                               .Build();
  bookmarks_menu.BuildBookmarksActions(bookmarks_submenu.get());
  menu_root_->AddChild(std::move(bookmarks_submenu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0]->GetTitle(), u"Google Doc");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"google.com");

  menu_root_->ResetActionList();
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

  // The zoom percentage is a read-only label and must not be indexed.
  auto zoom_label = actions::ActionItem::Builder()
                        .SetActionId(kActionZoomNormal)
                        .SetText(u"100%")
                        .Build();
  zoom_label->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                          AppMenuActionItem::DisplayType::kLabel);
  zoom_submenu->AddChild(std::move(zoom_label));

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

TEST_F(AppMenuSearchControllerTest, GetSearchItemTypeForBookmarksAndTabGroups) {
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

  auto tab_groups_menu = actions::ActionItem::Builder()
                             .SetActionId(kActionSavedTabGroupsSubmenu)
                             .SetText(u"Tab Groups")
                             .Build();

  tab_groups_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"Research Group").Build());

  menu_root_->AddChild(std::move(tab_groups_menu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 3u);

  EXPECT_EQ(items[0]->GetTitle(), u"Bookmark Manager");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Bookmarks");

  EXPECT_EQ(items[1]->GetTitle(), u"Google Doc");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(items[1]->GetSecondaryText(), u"Work");

  EXPECT_EQ(items[2]->GetTitle(), u"Research Group");
  EXPECT_EQ(items[2]->GetType(), AppMenuSearchItem::Type::kTabGroup);
  EXPECT_TRUE(items[2]->GetSecondaryText().empty());
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
  group_entity->AddChild(
      actions::ActionItem::Builder().SetText(u"Docs tab").Build());
  tab_groups_menu->AddChild(std::move(group_entity));
  menu_root_->AddChild(std::move(tab_groups_menu));

  controller_->InitializeSearchIndex();

  // Each saved group is one result. The tab group actions (including "Create
  // new tab group") and the group's tabs are not indexed.
  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 3u);

  EXPECT_EQ(items[0]->GetTitle(), u"Bookmark this tab");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Bookmarks");

  EXPECT_EQ(items[1]->GetTitle(), u"Amazon");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(items[1]->GetSecondaryText(), u"Bookmarks");

  EXPECT_EQ(items[2]->GetTitle(), u"Work");
  EXPECT_EQ(items[2]->GetType(), AppMenuSearchItem::Type::kTabGroup);
  EXPECT_TRUE(items[2]->GetSecondaryText().empty());
}

TEST_F(AppMenuSearchControllerTest, SearchRequiresInitializedIndex) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());

  // TODO(crbug.com/41487832): Enable on ChromeOS when test setup in the death
  // subprocess is fixed. AshTestHelper causes ICU/Timezone crashes in the
  // forked process before the CHECK can fire.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_CHECK_DEATH(controller_->Search(u"New"));
#endif

  controller_->InitializeSearchIndex();
  EXPECT_NE(controller_->Search(u"New"), nullptr);
}

TEST_F(AppMenuSearchControllerTest, SearchResultsCategorizationAndCapping) {
  for (int i = 0; i < 8; ++i) {
    menu_root_->AddChild(
        actions::ActionItem::Builder()
            .SetActionId(kActionNewTab)
            .SetText(u"Action Item " + base::NumberToString16(i))
            .Build());
  }

  auto bookmarks_menu = actions::ActionItem::Builder()
                            .SetActionId(kActionBookmarksSubmenu)
                            .SetText(u"Bookmarks")
                            .Build();
  for (int i = 0; i < 5; ++i) {
    bookmarks_menu->AddChild(
        actions::ActionItem::Builder()
            .SetText(u"Bookmark Item " + base::NumberToString16(i))
            .Build());
  }
  menu_root_->AddChild(std::move(bookmarks_menu));

  auto recent_tabs_menu = actions::ActionItem::Builder()
                              .SetActionId(kActionRecentTabsSubmenu)
                              .SetText(u"History")
                              .Build();
  std::vector<RecentTabItem> recent_tabs;
  for (int i = 0; i < 4; ++i) {
    recent_tabs.push_back(
        MakeRecentTab(u"Recent Tab Item " + base::NumberToString16(i),
                      "https://example.com/" + base::NumberToString(i)));
  }
  recent_tabs_menu_.CreateRecentTabsActionForTesting(recent_tabs_menu.get(),
                                                     recent_tabs);
  menu_root_->AddChild(std::move(recent_tabs_menu));

  controller_->InitializeSearchIndex();
  const std::unique_ptr<actions::ActionItem> results_root =
      controller_->Search(u"Item");
  ASSERT_NE(results_root, nullptr);

  const auto& sections = results_root->GetChildren().children();
  ASSERT_EQ(sections.size(), 8u);

  auto* bookmarks_section = sections[6]->GetActionItem();
  EXPECT_EQ(bookmarks_section->GetChildren().children().size(), 4u);

  auto* recent_tabs_section = sections[7]->GetActionItem();
  EXPECT_EQ(recent_tabs_section->GetChildren().children().size(), 4u);
}

TEST_F(AppMenuSearchControllerTest, SearchResultsTreeCategoryHeaders) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());

  auto bookmarks_menu = actions::ActionItem::Builder()
                            .SetActionId(kActionBookmarksSubmenu)
                            .SetText(u"Bookmarks")
                            .Build();
  bookmarks_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"New Bookmark").Build());
  menu_root_->AddChild(std::move(bookmarks_menu));

  auto tab_groups_menu = actions::ActionItem::Builder()
                             .SetActionId(kActionSavedTabGroupsSubmenu)
                             .SetText(u"Tab Groups")
                             .Build();
  tab_groups_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"New Tab Group").Build());
  menu_root_->AddChild(std::move(tab_groups_menu));

  auto recent_tabs_menu = actions::ActionItem::Builder()
                              .SetActionId(kActionRecentTabsSubmenu)
                              .SetText(u"History")
                              .Build();
  recent_tabs_menu_.CreateRecentTabsActionForTesting(
      recent_tabs_menu.get(),
      {MakeRecentTab(u"New Recent Tab", "https://example.com/")});
  menu_root_->AddChild(std::move(recent_tabs_menu));

  controller_->InitializeSearchIndex();
  const std::unique_ptr<actions::ActionItem> results_root =
      controller_->Search(u"New");
  ASSERT_NE(results_root, nullptr);

  const auto& children = results_root->GetChildren().children();
  ASSERT_EQ(children.size(), 4u);

  EXPECT_EQ(children[0]->GetActionItem()->GetText(), u"New Tab");

  auto verify_section = [&](actions::ActionItem* section,
                            int expected_header_id,
                            const std::u16string& expected_item_text) {
    ASSERT_NE(section, nullptr);
    const std::u16string expected_header =
        l10n_util::GetStringUTF16(expected_header_id);
    EXPECT_EQ(section->GetText(), expected_header);
    EXPECT_EQ(section->GetProperty(AppMenuActionItem::kDisplayTypeKey),
              AppMenuActionItem::DisplayType::kSection);

    const auto& section_children = section->GetChildren().children();
    ASSERT_EQ(section_children.size(), 2u);
    EXPECT_EQ(section_children[0]->GetActionItem()->GetText(), expected_header);
    EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                  AppMenuActionItem::kDisplayTypeKey),
              AppMenuActionItem::DisplayType::kHeader);
    EXPECT_EQ(section_children[1]->GetActionItem()->GetText(),
              expected_item_text);
  };

  verify_section(children[1]->GetActionItem(),
                 IDS_APP_MENU_SEARCH_BOOKMARKS_HEADER, u"New Bookmark");
  verify_section(children[2]->GetActionItem(),
                 IDS_APP_MENU_SEARCH_TAB_GROUPS_HEADER, u"New Tab Group");
  verify_section(children[3]->GetActionItem(),
                 IDS_APP_MENU_SEARCH_RECENT_TABS_HEADER, u"New Recent Tab");
}

TEST_F(AppMenuSearchControllerTest,
       BuildSearchResultsTreeAttachesSecondaryText) {
  auto bookmarks_menu = actions::ActionItem::Builder()
                            .SetActionId(kActionBookmarksSubmenu)
                            .SetText(u"Bookmarks")
                            .Build();
  bookmarks_menu->AddChild(
      actions::ActionItem::Builder().SetText(u"Work").Build());
  menu_root_->AddChild(std::move(bookmarks_menu));

  controller_->InitializeSearchIndex();
  const std::unique_ptr<actions::ActionItem> results_root =
      controller_->Search(u"Work");
  ASSERT_NE(results_root, nullptr);

  bool found_secondary_text = false;
  for (const auto& section : results_root->GetChildren().children()) {
    for (const auto& item : section->GetChildren().children()) {
      std::u16string* secondary_text =
          item->GetProperty(AppMenuActionItem::kSecondaryTextKey);
      if (secondary_text && !secondary_text->empty()) {
        found_secondary_text = true;
        EXPECT_EQ(*secondary_text, u"Bookmarks");
        break;
      }
    }
  }

  EXPECT_TRUE(found_secondary_text);
}

TEST_F(AppMenuSearchControllerTest,
       BuildSearchResultsTreeAttachesTitleAndIconOverride) {
  const ui::ImageModel icon = ui::ImageModel::FromVectorIcon(kTabIcon);
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionToggleVerticalTabs)
                       .SetText(u"Show tabs vertically")
                       .Build());
  menu_root_->AddChild(AppMenuActionItem::CreateIndirect(
      kActionToggleVerticalTabs, scope_.get(),
      {
          .text_override = u"Show tabs horizontally",
          .icon_override = icon,
      }));

  controller_->InitializeSearchIndex();
  const std::unique_ptr<actions::ActionItem> results_root =
      controller_->Search(u"horizontal");
  ASSERT_NE(results_root, nullptr);
  const auto& rows = results_root->GetChildren().children();
  ASSERT_EQ(rows.size(), 1u);
  const actions::BaseAction* const row = rows[0].get();

  ASSERT_NE(row->GetProperty(AppMenuActionItem::kTextOverrideKey), nullptr);
  EXPECT_EQ(*row->GetProperty(AppMenuActionItem::kTextOverrideKey),
            u"Show tabs horizontally");
  ASSERT_NE(row->GetProperty(AppMenuActionItem::kIconOverrideKey), nullptr);
  EXPECT_EQ(*row->GetProperty(AppMenuActionItem::kIconOverrideKey), icon);
}

TEST_F(AppMenuSearchControllerTest, BuildSearchResultsTreePreservesCheckable) {
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionBookmarkBarSubmenuAlwaysShow)
                       .SetText(u"Always show")
                       .SetChecked(true)
                       .Build());
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionShowBookmarkBar)
                       .SetText(u"Always show bookmarks bar")
                       .Build());
  menu_root_->AddChild(
      AppMenuActionItem::CreateIndirect(kActionBookmarkBarSubmenuAlwaysShow,
                                        scope_.get(), {.is_checkable = true}));
  menu_root_->AddChild(
      AppMenuActionItem::CreateIndirect(kActionShowBookmarkBar, scope_.get()));

  controller_->InitializeSearchIndex();
  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 2u);
  EXPECT_TRUE(items[0]->IsCheckable());
  EXPECT_FALSE(items[1]->IsCheckable());

  const std::unique_ptr<actions::ActionItem> results_root =
      controller_->Search(u"Always show");
  ASSERT_NE(results_root, nullptr);
  const auto& rows = results_root->GetChildren().children();
  ASSERT_EQ(rows.size(), 2u);

  for (const auto& row : rows) {
    const bool is_checkable_row = row->GetActionItem()->GetActionId() ==
                                  kActionBookmarkBarSubmenuAlwaysShow;
    EXPECT_EQ(row->GetProperty(AppMenuActionItem::kIsCheckableKey),
              is_checkable_row);
    EXPECT_EQ(row->GetActionItem()->GetChecked(), is_checkable_row);
  }
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

  // Built-in command:
  recent_tabs_menu->AddChild(actions::ActionItem::Builder()
                                 .SetActionId(kActionShowHistory)
                                 .SetText(u"Open history page")
                                 .Build());

  // Dynamic entries, as built by RecentTabsDynamicMenu: a section header, a
  // single closed tab, a closed window (restore command, divider and its tabs)
  // and a remote device with its tabs.
  RecentTabItem window(RecentTabItem::Type::kWindow, u"3 tabs");
  window.add_child(
      RecentTabItem(RecentTabItem::Type::kCommand, u"Restore window"));
  window.add_child(RecentTabItem(RecentTabItem::Type::kDivider, u""));
  window.add_child(MakeRecentTab(u"Window Tab 1", "https://www.example.com/a"));
  window.add_child(MakeRecentTab(u"Window Tab 2", "https://docs.google.com/b"));

  RecentTabItem device(RecentTabItem::Type::kDevice, u"Edward's Android");
  device.add_child(MakeRecentTab(u"Phone Tab 1", "https://en.wikipedia.org/x"));
  device.add_child(MakeRecentTab(u"Phone Tab 2", "https://youtube.com/y"));

  recent_tabs_menu_.CreateRecentTabsActionForTesting(
      recent_tabs_menu.get(),
      {RecentTabItem(RecentTabItem::Type::kHeader, u"Recently closed"),
       MakeRecentTab(u"GitHub", "https://github.com/foo"), std::move(window),
       std::move(device)});

  menu_root_->AddChild(std::move(recent_tabs_menu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 6u);

  // Built-in command:
  EXPECT_EQ(items[0]->GetTitle(), u"Open history page");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"History");

  // Tabs show their domain. "Restore window", the header and the divider are
  // not indexed.
  EXPECT_EQ(items[1]->GetTitle(), u"GitHub");
  EXPECT_EQ(items[1]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[1]->GetSecondaryText(), u"github.com");

  EXPECT_EQ(items[2]->GetTitle(), u"Window Tab 1");
  EXPECT_EQ(items[2]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[2]->GetSecondaryText(), u"example.com");

  EXPECT_EQ(items[3]->GetTitle(), u"Window Tab 2");
  EXPECT_EQ(items[3]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[3]->GetSecondaryText(), u"docs.google.com");

  EXPECT_EQ(items[4]->GetTitle(), u"Phone Tab 1");
  EXPECT_EQ(items[4]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[4]->GetSecondaryText(), u"en.wikipedia.org");

  EXPECT_EQ(items[5]->GetTitle(), u"Phone Tab 2");
  EXPECT_EQ(items[5]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[5]->GetSecondaryText(), u"youtube.com");
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

  // Empty folder explicitly marked with `kIsSubmenuKey` (matching empty
  // bookmark folders created by
  // `BookmarksDynamicMenu::AddBookmarkFolderAction`) should not be indexed as a
  // leaf.
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetText(u"Empty Bookmark Folder")
                           .SetProperty(AppMenuActionItem::kIsSubmenuKey, true)
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

  // Add a non-actionable child (kSearch DisplayType on the IndirectActionItem)
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

  // Verifies kTextOverrideKey and kDisplayTypeKey were read from the
  // IndirectActionItem wrapper for the leaf title, parent submenu breadcrumb,
  // and search filtering.
  EXPECT_EQ(items[0]->GetTitle(), u"Hide bookmark bar");
  EXPECT_EQ(items[0]->GetSecondaryText(), u"Bookmarks and lists");
}

TEST_F(AppMenuSearchControllerTest, ChangingQueryReplacesResults) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionPrint)
                           .SetText(u"Print")
                           .Build());
  controller_->InitializeSearchIndex();

  base::WeakPtr<actions::ActionItem> first_results;
  EXPECT_CALL(delegate_, ShowSearchResults(testing::NotNull(), testing::_))
      .WillOnce(
          [&first_results](actions::ActionItem* results,
                           std::unique_ptr<actions::ActionItem> old_results) {
            first_results = results->GetAsWeakPtr();
          })
      .WillOnce(
          [&first_results](actions::ActionItem* results,
                           std::unique_ptr<actions::ActionItem> old_results) {
            // The previous results must outlive the call that replaces them, so
            // the menu can remove the rows pointing into them first.
            EXPECT_TRUE(first_results);
            EXPECT_EQ(first_results.get(), old_results.get());
            EXPECT_NE(results, first_results.get());
          });
  search_bar_->SetText(u"New");
  search_bar_->SetText(u"Print");
  // The previous results are freed once the menu shows the new ones.
  EXPECT_FALSE(first_results);
}

TEST_F(AppMenuSearchControllerTest, ShortQueryKeepsResults) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());
  controller_->InitializeSearchIndex();

  // A query below the minimum length shows no results.
  search_bar_->SetText(u"N");

  base::WeakPtr<actions::ActionItem> results;
  EXPECT_CALL(delegate_, ShowSearchResults(testing::NotNull(), testing::_))
      .WillOnce([&results](actions::ActionItem* shown,
                           std::unique_ptr<actions::ActionItem> old_results) {
        results = shown->GetAsWeakPtr();
      });
  search_bar_->SetText(u"New");
  EXPECT_TRUE(results);

  // Shortening the query below the minimum length keeps the results.
  search_bar_->SetText(u"N");
  EXPECT_TRUE(results);
}

TEST_F(AppMenuSearchControllerTest, EmptyQueryRestoresMenu) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());
  controller_->InitializeSearchIndex();

  base::WeakPtr<actions::ActionItem> results;
  testing::InSequence sequence;
  EXPECT_CALL(delegate_, ShowSearchResults(testing::NotNull(), testing::_))
      .WillOnce([&results](actions::ActionItem* shown,
                           std::unique_ptr<actions::ActionItem> old_results) {
        results = shown->GetAsWeakPtr();
      });
  EXPECT_CALL(delegate_, RestoreMenu(testing::_))
      .WillOnce([&results](std::unique_ptr<actions::ActionItem> old_results) {
        // The results must outlive the call so the menu can remove the rows
        // pointing into them first.
        EXPECT_TRUE(results);
        EXPECT_EQ(results.get(), old_results.get());
      });
  search_bar_->SetText(u"New");
  search_bar_->SetText(std::u16string());
  // The results are freed once the menu is restored.
  EXPECT_FALSE(results);
}

TEST_F(AppMenuSearchControllerTest, WhitespaceQueryRestoresMenu) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());
  controller_->InitializeSearchIndex();

  testing::InSequence sequence;
  EXPECT_CALL(delegate_, ShowSearchResults(testing::NotNull(), testing::_));
  EXPECT_CALL(delegate_, RestoreMenu(testing::_));
  search_bar_->SetText(u"New");
  search_bar_->SetText(u"   ");
}

TEST_F(AppMenuSearchControllerTest, ClearButtonRestoresMenu) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());
  controller_->InitializeSearchIndex();

  testing::InSequence sequence;
  EXPECT_CALL(delegate_, ShowSearchResults(testing::NotNull(), testing::_));
  EXPECT_CALL(delegate_, RestoreMenu(testing::_));
  search_bar_->SetText(u"New");
  views::test::ButtonTestApi(
      views::Button::AsButton(search_bar_->GetViewByElementId(
          AppMenuSearchBarView::kClearButtonElementId)))
      .NotifyDefaultMouseClick();
  EXPECT_TRUE(search_bar_->GetText().empty());
  EXPECT_TRUE(
      search_bar_
          ->GetViewByElementId(AppMenuSearchBarView::kSearchIconElementId)
          ->GetVisible());
}

TEST_F(AppMenuSearchControllerTest, EmptyQueryWithoutResultsDoesNothing) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());
  controller_->InitializeSearchIndex();

  // `delegate_` is a StrictMock, so any delegate call fails the test.
  search_bar_->SetText(u"N");
  search_bar_->SetText(std::u16string());
}

TEST_F(AppMenuSearchControllerTest, OtherDeviceTabsSubmenuIndexed) {
  auto other_device_tabs_menu = actions::ActionItem::Builder()
                                    .SetActionId(kActionOtherDeviceTabsSubmenu)
                                    .SetText(u"Other device tabs")
                                    .Build();

  recent_tabs_menu_.CreateRecentTabsActionForTesting(
      other_device_tabs_menu.get(),
      {RecentTabItem(RecentTabItem::Type::kHeader, u"Pixel 9"),
       MakeRecentTab(u"Synced Page", "https://example.org/page")});

  menu_root_->AddChild(std::move(other_device_tabs_menu));

  controller_->InitializeSearchIndex();

  const auto& items = controller_->search_items_for_testing();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0]->GetTitle(), u"Synced Page");
  EXPECT_EQ(items[0]->GetType(), AppMenuSearchItem::Type::kRecentTabs);
  EXPECT_EQ(items[0]->GetSecondaryText(), u"example.org");
}

TEST_F(AppMenuSearchControllerTest, BuildSearchResultsTreeHighlightRange) {
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewTab)
                           .SetText(u"New Tab")
                           .Build());
  menu_root_->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionNewIncognitoWindow)
                           .SetText(u"New Incognito Window")
                           .AddSynonyms({u"private"})
                           .Build());
  scope_->AddChild(actions::ActionItem::Builder()
                       .SetActionId(kActionToggleVerticalTabs)
                       .SetText(u"Show tabs vertically")
                       .Build());
  menu_root_->AddChild(AppMenuActionItem::CreateIndirect(
      kActionToggleVerticalTabs, scope_.get(),
      {.text_override = u"Show tabs &horizontally"}));
  auto rd_submenu = actions::ActionItem::Builder().SetText(u"R&&D").Build();
  rd_submenu->AddChild(actions::ActionItem::Builder()
                           .SetActionId(kActionShowChromeLabs)
                           .SetText(u"Q&&A Portal")
                           .AddSynonyms({u"faq"})
                           .Build());
  menu_root_->AddChild(std::move(rd_submenu));

  controller_->InitializeSearchIndex();

  // Case-insensitive word-prefix match sets highlight range.
  {
    const std::unique_ptr<actions::ActionItem> results =
        controller_->Search(u"  tab  ");
    ASSERT_NE(results, nullptr);
    const auto& rows = results->GetChildren().children();
    ASSERT_FALSE(rows.empty());
    const gfx::Range* range =
        rows[0]->GetProperty(AppMenuActionItem::kHighlightRangeKey);
    ASSERT_NE(range, nullptr);
    EXPECT_EQ(*range, gfx::Range(4, 7));
  }

  // Case-insensitive inner-word substring match sets highlight range.
  {
    const std::unique_ptr<actions::ActionItem> results =
        controller_->Search(u"cognito");
    ASSERT_NE(results, nullptr);
    const auto& rows = results->GetChildren().children();
    ASSERT_EQ(rows.size(), 1u);
    const gfx::Range* range =
        rows[0]->GetProperty(AppMenuActionItem::kHighlightRangeKey);
    ASSERT_NE(range, nullptr);
    EXPECT_EQ(*range, gfx::Range(6, 13));
  }

  // Text override strips mnemonic '&' and computes accurate highlight range.
  {
    const std::unique_ptr<actions::ActionItem> results =
        controller_->Search(u"horizontal");
    ASSERT_NE(results, nullptr);
    const auto& rows = results->GetChildren().children();
    ASSERT_EQ(rows.size(), 1u);
    const std::u16string* text_override =
        rows[0]->GetProperty(AppMenuActionItem::kTextOverrideKey);
    ASSERT_NE(text_override, nullptr);
    EXPECT_EQ(*text_override, u"Show tabs horizontally");
    const gfx::Range* range =
        rows[0]->GetProperty(AppMenuActionItem::kHighlightRangeKey);
    ASSERT_NE(range, nullptr);
    EXPECT_EQ(*range, gfx::Range(10, 20));
  }

  // Fuzzy transposition match ("atb" -> "New Tab") does not set highlight
  // range.
  {
    const std::unique_ptr<actions::ActionItem> results =
        controller_->Search(u"atb");
    ASSERT_NE(results, nullptr);
    const auto& rows = results->GetChildren().children();
    ASSERT_FALSE(rows.empty());
    EXPECT_EQ(rows[0]->GetProperty(AppMenuActionItem::kHighlightRangeKey),
              nullptr);
  }

  // Synonym-only match ("private" -> "New Incognito Window") does not set
  // highlight range.
  {
    const std::unique_ptr<actions::ActionItem> results =
        controller_->Search(u"private");
    ASSERT_NE(results, nullptr);
    const auto& rows = results->GetChildren().children();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]->GetProperty(AppMenuActionItem::kHighlightRangeKey),
              nullptr);
  }

  // Literal '&' (escaped as "&&" on the ActionItem) in both title and
  // breadcrumb is unescaped when highlighted (where mnemonics are disabled) and
  // re-escaped when unhighlighted so MenuItemView does not strip '&'.
  {
    const std::unique_ptr<actions::ActionItem> highlighted_results =
        controller_->Search(u"portal");
    ASSERT_NE(highlighted_results, nullptr);
    const auto& highlighted_rows =
        highlighted_results->GetChildren().children();
    ASSERT_EQ(highlighted_rows.size(), 1u);
    ASSERT_NE(
        highlighted_rows[0]->GetProperty(AppMenuActionItem::kTextOverrideKey),
        nullptr);
    EXPECT_EQ(
        *highlighted_rows[0]->GetProperty(AppMenuActionItem::kTextOverrideKey),
        u"Q&A Portal");
    ASSERT_NE(
        highlighted_rows[0]->GetProperty(AppMenuActionItem::kSecondaryTextKey),
        nullptr);
    EXPECT_EQ(
        *highlighted_rows[0]->GetProperty(AppMenuActionItem::kSecondaryTextKey),
        u"R&D");
    const gfx::Range* range =
        highlighted_rows[0]->GetProperty(AppMenuActionItem::kHighlightRangeKey);
    ASSERT_NE(range, nullptr);
    EXPECT_EQ(*range, gfx::Range(4, 10));

    const std::unique_ptr<actions::ActionItem> unhighlighted_results =
        controller_->Search(u"faq");
    ASSERT_NE(unhighlighted_results, nullptr);
    const auto& unhighlighted_rows =
        unhighlighted_results->GetChildren().children();
    ASSERT_EQ(unhighlighted_rows.size(), 1u);
    ASSERT_NE(
        unhighlighted_rows[0]->GetProperty(AppMenuActionItem::kTextOverrideKey),
        nullptr);
    EXPECT_EQ(*unhighlighted_rows[0]->GetProperty(
                  AppMenuActionItem::kTextOverrideKey),
              u"Q&&A Portal");
    ASSERT_NE(unhighlighted_rows[0]->GetProperty(
                  AppMenuActionItem::kSecondaryTextKey),
              nullptr);
    EXPECT_EQ(*unhighlighted_rows[0]->GetProperty(
                  AppMenuActionItem::kSecondaryTextKey),
              u"R&&D");
    EXPECT_EQ(unhighlighted_rows[0]->GetProperty(
                  AppMenuActionItem::kHighlightRangeKey),
              nullptr);
  }
}

// Fixture for verifying that the fuzzy search scoring params, configured via
// the kChroMenuSearch feature params or the --chro-menu-search-scoring switch
// (chrome://flags), change the ranking of search results.
class AppMenuSearchControllerScoringTest : public AppMenuSearchControllerTest {
 protected:
  // Adds two actions that both match "passkey": one via its title and one via
  // a synonym, so their relative order is decided by the field weights.
  void AddTitleAndSynonymActions() {
    menu_root_->AddChild(actions::ActionItem::Builder()
                             .SetActionId(kActionNewTab)
                             .SetText(u"Google Password Manager")
                             .AddSynonyms({u"passkey"})
                             .Build());
    menu_root_->AddChild(actions::ActionItem::Builder()
                             .SetActionId(kActionPrint)
                             .SetText(u"Passkey Settings")
                             .Build());
  }

  // Builds the search index, searches for "passkey" and returns the titles of
  // the action results in ranked order.
  std::vector<std::u16string> SearchPasskeyTitles() {
    controller_->InitializeSearchIndex();
    const std::unique_ptr<actions::ActionItem> results_root =
        controller_->Search(u"passkey");
    std::vector<std::u16string> titles;
    if (!results_root) {
      return titles;
    }
    for (const auto& child : results_root->GetChildren().children()) {
      titles.emplace_back(child->GetActionItem()->GetText());
    }
    return titles;
  }

  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppMenuSearchControllerScoringTest, DefaultWeightsRankTitleFirst) {
  AddTitleAndSynonymActions();
  EXPECT_THAT(
      SearchPasskeyTitles(),
      testing::ElementsAre(u"Passkey Settings", u"Google Password Manager"));
}

TEST_F(AppMenuSearchControllerScoringTest, FeatureParamsChangeRanking) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kChroMenuSearch,
      {{"title_weight", "0.8"}, {"synonym_weight", "1.0"}});
  AddTitleAndSynonymActions();
  EXPECT_THAT(
      SearchPasskeyTitles(),
      testing::ElementsAre(u"Google Password Manager", u"Passkey Settings"));
}

TEST_F(AppMenuSearchControllerScoringTest, FlagSwitchChangesRanking) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kChroMenuSearchScoring, "0.8,0.8,1.0");
  AddTitleAndSynonymActions();
  EXPECT_THAT(
      SearchPasskeyTitles(),
      testing::ElementsAre(u"Google Password Manager", u"Passkey Settings"));
}

TEST_F(AppMenuSearchControllerScoringTest, FlagSwitchOverridesFeatureParams) {
  // Feature params favor synonyms, but the switch restores title priority.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kChroMenuSearch,
      {{"title_weight", "0.8"}, {"synonym_weight", "1.0"}});
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kChroMenuSearchScoring, "1.0,0.8,0.85");
  AddTitleAndSynonymActions();
  EXPECT_THAT(
      SearchPasskeyTitles(),
      testing::ElementsAre(u"Passkey Settings", u"Google Password Manager"));
}

TEST_F(AppMenuSearchControllerScoringTest,
       FlagSwitchEmptyOrInvalidEntriesKeepDefaults) {
  // Only the synonym weight is overridden; the empty and invalid title and
  // secondary entries keep their defaults (title 1.0). Synonym 0.5 drops the
  // synonym match below the minimum score.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kChroMenuSearchScoring, ",abc,0.5");
  AddTitleAndSynonymActions();
  EXPECT_THAT(SearchPasskeyTitles(), testing::ElementsAre(u"Passkey Settings"));
}

TEST_F(AppMenuSearchControllerScoringTest, MinScoreFeatureParamFiltersResults) {
  // The synonym match scores ~0.95 * 0.85 = ~0.81, below a 0.9 cutoff, while
  // the title match (~0.95) stays above it.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kChroMenuSearch, {{"min_score", "0.9"}});
  AddTitleAndSynonymActions();
  EXPECT_THAT(SearchPasskeyTitles(), testing::ElementsAre(u"Passkey Settings"));
}

TEST_F(AppMenuSearchControllerScoringTest, MinScoreFlagSwitchFiltersResults) {
  // Only the fourth (min_score) entry is set; the weights keep their
  // defaults.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kChroMenuSearchScoring, ",,,0.9");
  AddTitleAndSynonymActions();
  EXPECT_THAT(SearchPasskeyTitles(), testing::ElementsAre(u"Passkey Settings"));
}

}  // namespace
