// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include <utility>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/split_tab_mute_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_swap_menu_model.h"
#include "chrome/browser/ui/tabs/split_view_layout_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/testing_pref_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

class TabMenuModelBrowserTest : public MenuModelTest,
                                public InProcessBrowserTest {
 public:
  TabMenuModelBrowserTest() {
    // TODO(crbug.com/557287887): Fix SplitViewLayoutMenuModel so that it
    // doesn't break TabMenuModelBrowserTest.Basics.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{tabs::kSplitViewHorizontal,
                               {{"split_view_horizontal_direct_tab_access",
                                 "false"}}}},
        /*disabled_features=*/{});
  }

  Profile* profile() { return browser()->GetProfile(); }

  void ActivateSwapWithSplitSubmenuCommand(
      int tab_index,
      SplitTabSwapMenuModel::CommandId command_id) {
    TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                      browser()->tab_strip_model(), tab_index);
    size_t submenu_index =
        menu.GetIndexOfCommandId(TabStripModel::CommandSwapWithActiveSplit)
            .value();
    ui::SimpleMenuModel* submenu = static_cast<ui::SimpleMenuModel*>(
        menu.GetSubmenuModelAt(submenu_index));
    submenu->ActivatedAt(static_cast<size_t>(
        submenu->GetIndexOfCommandId(static_cast<int>(command_id)).value()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, Basics) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  TabMenuModel model(&delegate_, TabMenuModelDelegate::From(browser()),
                     browser()->tab_strip_model(), 0);

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 5u);

  int item_count = 0;
  CountEnabledExecutable(&model, &item_count);
  EXPECT_GT(item_count, 0);
  EXPECT_EQ(item_count, delegate_.execute_count_);
  EXPECT_EQ(item_count, delegate_.enable_count_);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, MoveToNewWindow) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  TabMenuModel model(&delegate_, TabMenuModelDelegate::From(browser()),
                     browser()->tab_strip_model(), 0);

  // Verify that CommandMoveTabsToNewWindow is in the menu.
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, AddToExistingGroupSubmenu) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddToNewGroup({0});
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    tab_strip_model, 3);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 5u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetCommandIdAt(2),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 1);
  EXPECT_FALSE(submenu->GetIconAt(2).IsEmpty());
  EXPECT_EQ(submenu->GetCommandIdAt(3),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 2);
  EXPECT_EQ(submenu->GetCommandIdAt(4),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 3);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest,
                       AddToExistingGroupSubmenu_DoesNotIncludeCurrentGroup) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddToNewGroup({0});
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    tab_strip_model, 1);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 4u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetCommandIdAt(2),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 1);
  EXPECT_FALSE(submenu->GetIconAt(2).IsEmpty());
  EXPECT_EQ(submenu->GetCommandIdAt(3),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 2);
}

// In some cases, groups may change after the menu is created. For example an
// extension may modify groups while the menu is open. If a group referenced in
// the menu goes away, ensure we handle this gracefully.
//
// Regression test for crbug.com/40055511
IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest,
                       AddToExistingGroupAfterGroupDestroyed) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddToNewGroup({0});

  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    tab_strip_model, 1);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 3u);

  // Ungroup the tab at 0 to make the group in the menu dangle.
  tab_strip_model->RemoveFromGroup({0});

  // Try adding to the group from the menu.
  submenu->ActivatedAt(2);

  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(0).has_value());
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(1).has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, ActiveTabNotSplit) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 4);
  EXPECT_EQ(tab_strip_model->active_index(), 3);

  tab_strip_model->ExecuteContextMenuCommand(2,
                                             TabStripModel::CommandAddToSplit);
  tab_strip_model->ActivateTabAt(0);

  // Active tab is not split, context menu index is active tab
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 0);

    EXPECT_TRUE(menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit)
                    .has_value());
    EXPECT_FALSE(
        menu_model
            .GetIndexOfCommandId(TabStripModel::CommandSwapWithActiveSplit)
            .has_value());
    EXPECT_FALSE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandArrangeSplit)
            .has_value());
  }

  // Active tab is not split, context menu index is on inactive tab
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 1);

    EXPECT_TRUE(menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit)
                    .has_value());
    EXPECT_FALSE(
        menu_model
            .GetIndexOfCommandId(TabStripModel::CommandSwapWithActiveSplit)
            .has_value());
    EXPECT_FALSE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandArrangeSplit)
            .has_value());
  }

  // Active tab is not split, context menu index is on inactive split tab
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 2);

    EXPECT_FALSE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit)
            .has_value());
    EXPECT_FALSE(
        menu_model
            .GetIndexOfCommandId(TabStripModel::CommandSwapWithActiveSplit)
            .has_value());
    EXPECT_TRUE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandArrangeSplit)
            .has_value());
  }
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, SplitActiveTab) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 4);
  EXPECT_EQ(tab_strip_model->active_index(), 3);

  tab_strip_model->ExecuteContextMenuCommand(2,
                                             TabStripModel::CommandAddToSplit);

  // Active tab is split, context menu index is active tab
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 3);

    EXPECT_FALSE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit)
            .has_value());
    EXPECT_FALSE(
        menu_model
            .GetIndexOfCommandId(TabStripModel::CommandSwapWithActiveSplit)
            .has_value());
    EXPECT_TRUE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandArrangeSplit)
            .has_value());
  }

  // Active tab is split, context menu index is on inactive tab
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 1);

    EXPECT_FALSE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit)
            .has_value());
    EXPECT_TRUE(
        menu_model
            .GetIndexOfCommandId(TabStripModel::CommandSwapWithActiveSplit)
            .has_value());
    EXPECT_FALSE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandArrangeSplit)
            .has_value());
  }
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, MultiSelectTabs) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 4);
  EXPECT_EQ(tab_strip_model->active_index(), 3);

  tab_strip_model->ActivateTabAt(1);
  tab_strip_model->AddSelectionFromAnchorTo(2);

  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 2);

    auto index =
        menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit);

    EXPECT_TRUE(index.has_value());
    EXPECT_TRUE(menu_model.IsEnabledAt(index.value()));
  }

  tab_strip_model->ActivateTabAt(0);
  tab_strip_model->AddSelectionFromAnchorTo(2);

  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 2);

    auto index =
        menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit);

    EXPECT_TRUE(index.has_value());
    EXPECT_FALSE(menu_model.IsEnabledAt(index.value()));
  }
}

namespace {

// Returns the label of the "Mute sites" item in `menu_model`.
std::u16string GetMuteSitesLabel(TabMenuModel& menu_model) {
  return menu_model.GetLabelAt(
      menu_model.GetIndexOfCommandId(TabStripModel::CommandToggleSiteMuted)
          .value());
}

// Returns the "Mute sites" submenu of `menu_model`, or nullptr if "Mute sites"
// is a regular menu item.
ui::SimpleMenuModel* GetMuteSitesSubmenu(TabMenuModel& menu_model) {
  const std::optional<size_t> index =
      menu_model.GetIndexOfCommandId(TabStripModel::CommandToggleSiteMuted);
  if (!index.has_value() ||
      menu_model.GetTypeAt(*index) != ui::MenuModel::TYPE_SUBMENU) {
    return nullptr;
  }
  return static_cast<ui::SimpleMenuModel*>(
      menu_model.GetSubmenuModelAt(*index));
}

size_t GetMuteSubmenuIndex(ui::SimpleMenuModel* submenu,
                           SplitTabMuteMenuModel::CommandId id) {
  return submenu->GetIndexOfCommandId(static_cast<int>(id)).value();
}

std::u16string GetMuteSubmenuLabel(ui::SimpleMenuModel* submenu,
                                   SplitTabMuteMenuModel::CommandId id) {
  return submenu->GetLabelAt(GetMuteSubmenuIndex(submenu, id));
}

}  // namespace

class TabMenuModelMuteSitesBrowserTest : public TabMenuModelBrowserTest {
 public:
  void SetUpOnMainThread() override {
    TabMenuModelBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Navigates the first tab to `first_url`, opens `second_url` in a second tab
  // and puts both tabs in a side-by-side split. Returns the split's id.
  split_tabs::SplitTabId CreateSplit(const GURL& first_url,
                                     const GURL& second_url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), second_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    EXPECT_EQ(tab_strip_model->count(), 2);
    tab_strip_model->AddToNewSplit(
        {0},
        split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kSideBySide),
        split_tabs::SplitTabCreatedSource::kToolbarButton);
    return tab_strip_model->GetSplitForTab(1).value();
  }

  // Returns the tab indices of the first and second view of `split_id`.
  std::pair<int, int> GetSplitIndices(split_tabs::SplitTabId split_id) {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    const std::vector<tabs::TabInterface*> tabs_in_split =
        tab_strip_model->GetSplitData(split_id)->ListTabs();
    EXPECT_EQ(tabs_in_split.size(), 2u);
    return {tab_strip_model->GetIndexOfTab(tabs_in_split[0]),
            tab_strip_model->GetIndexOfTab(tabs_in_split[1])};
  }

  bool IsMuted(int index) {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    return IsSiteMuted(*tab_strip_model, index) &&
           tab_strip_model->GetWebContentsAt(index)->IsAudioMuted();
  }
};

IN_PROC_BROWSER_TEST_F(TabMenuModelMuteSitesBrowserTest, MuteSitesSubmenu) {
  using CommandId = SplitTabMuteMenuModel::CommandId;

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Outside of a split, "Mute site" is a regular menu item.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 0);
    EXPECT_TRUE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandToggleSiteMuted)
            .has_value());
    EXPECT_FALSE(GetMuteSitesSubmenu(menu_model));
  }

  // Put two different sites in a split so that muting one of them leaves the
  // other one playing.
  const split_tabs::SplitTabId split_id =
      CreateSplit(embedded_test_server()->GetURL("a.com", "/title1.html"),
                  embedded_test_server()->GetURL("b.com", "/title2.html"));
  const auto [left_index, right_index] = GetSplitIndices(split_id);

  // For a split, "Mute sites" is a submenu that can mute both sites or just
  // the site in one of the views. Mute only the left site.
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    EXPECT_EQ(
        GetMuteSitesLabel(menu_model),
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_SOUND_MUTE_SITE, 2));
    ui::SimpleMenuModel* submenu = GetMuteSitesSubmenu(menu_model);
    ASSERT_TRUE(submenu);
    ASSERT_EQ(submenu->GetItemCount(), 4u);
    EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
    EXPECT_EQ(
        GetMuteSubmenuLabel(submenu, CommandId::kToggleAllSitesMuted),
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_SOUND_MUTE_SITE, 2));
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleStartSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_MUTE_LEFT_SITE));
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleEndSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_MUTE_RIGHT_SITE));

    submenu->ActivatedAt(
        GetMuteSubmenuIndex(submenu, CommandId::kToggleStartSiteMuted));
  }
  EXPECT_TRUE(IsMuted(left_index));
  EXPECT_FALSE(IsMuted(right_index));

  // The left site now offers to unmute, and "Mute sites" mutes both sites.
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    ui::SimpleMenuModel* submenu = GetMuteSitesSubmenu(menu_model);
    ASSERT_TRUE(submenu);
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleStartSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_UNMUTE_LEFT_SITE));
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleEndSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_MUTE_RIGHT_SITE));

    submenu->ActivatedAt(
        GetMuteSubmenuIndex(submenu, CommandId::kToggleAllSitesMuted));
  }
  EXPECT_TRUE(IsMuted(left_index));
  EXPECT_TRUE(IsMuted(right_index));

  // With both sites muted, the menu offers to unmute. Unmute the right site.
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, right_index);
    EXPECT_EQ(
        GetMuteSitesLabel(menu_model),
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_SOUND_UNMUTE_SITE, 2));
    ui::SimpleMenuModel* submenu = GetMuteSitesSubmenu(menu_model);
    ASSERT_TRUE(submenu);
    EXPECT_EQ(
        GetMuteSubmenuLabel(submenu, CommandId::kToggleAllSitesMuted),
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_SOUND_UNMUTE_SITE, 2));
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleEndSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_UNMUTE_RIGHT_SITE));

    submenu->ActivatedAt(
        GetMuteSubmenuIndex(submenu, CommandId::kToggleEndSiteMuted));
  }
  EXPECT_TRUE(IsMuted(left_index));
  EXPECT_FALSE(IsMuted(right_index));

  // In RTL, the start tab (index 0) in a side-by-side split is shown on the
  // right and the end tab (index 1) is shown on the left, so the labels flip
  // while `kToggleStartSiteMuted` and `kToggleEndSiteMuted` still target index
  // 0 and index 1 respectively.
  {
    base::i18n::ScopedDefaultIcuLocale rtl_locale(
        base::i18n::GetKnownLanguageTag("he"));
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    ui::SimpleMenuModel* submenu = GetMuteSitesSubmenu(menu_model);
    ASSERT_TRUE(submenu);
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleStartSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_UNMUTE_RIGHT_SITE));
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleEndSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_MUTE_LEFT_SITE));

    submenu->ActivatedAt(
        GetMuteSubmenuIndex(submenu, CommandId::kToggleEndSiteMuted));
    EXPECT_TRUE(IsMuted(right_index));
    submenu->ActivatedAt(
        GetMuteSubmenuIndex(submenu, CommandId::kToggleEndSiteMuted));
    EXPECT_FALSE(IsMuted(right_index));
  }

  // Stacked splits refer to the top and bottom views.
  tab_strip_model->UpdateSplitLayout(split_id,
                                     split_tabs::SplitTabLayout::kStacked);
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    ui::SimpleMenuModel* submenu = GetMuteSitesSubmenu(menu_model);
    ASSERT_TRUE(submenu);
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleStartSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_UNMUTE_TOP_SITE));
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleEndSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_MUTE_BOTTOM_SITE));
  }

  // An unselected split still gets the submenu, and "Mute sites" covers both
  // of its sites.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  ASSERT_FALSE(tab_strip_model->IsTabSelected(left_index));
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    EXPECT_EQ(
        GetMuteSitesLabel(menu_model),
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_SOUND_MUTE_SITE, 2));
    EXPECT_TRUE(GetMuteSitesSubmenu(menu_model));
  }

  // When the selection spans the split and another tab, "Mute sites" acts on
  // the whole selection, so it stays a regular menu item.
  tab_strip_model->AddSelectionFromAnchorTo(left_index);
  ASSERT_TRUE(tab_strip_model->IsTabSelected(left_index));
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    EXPECT_TRUE(
        menu_model.GetIndexOfCommandId(TabStripModel::CommandToggleSiteMuted)
            .has_value());
    EXPECT_FALSE(GetMuteSitesSubmenu(menu_model));
  }

  // After separating the split, the muted site can still be unmuted via the
  // regular single-tab "Unmute site" item.
  tab_strip_model->RemoveSplit(split_id);
  tab_strip_model->ActivateTabAt(left_index);
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    EXPECT_EQ(
        GetMuteSitesLabel(menu_model),
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_SOUND_UNMUTE_SITE, 1));
    tab_strip_model->ExecuteContextMenuCommand(
        left_index, TabStripModel::CommandToggleSiteMuted);
  }
  EXPECT_FALSE(IsMuted(left_index));
}

// The per-view items change the sound content setting of the view's site, so
// when both views show the same site they mute and unmute both views.
IN_PROC_BROWSER_TEST_F(TabMenuModelMuteSitesBrowserTest,
                       MuteSitesSubmenuSameSite) {
  using CommandId = SplitTabMuteMenuModel::CommandId;

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  const split_tabs::SplitTabId split_id =
      CreateSplit(embedded_test_server()->GetURL("a.com", "/title1.html"),
                  embedded_test_server()->GetURL("a.com", "/title2.html"));
  const auto [left_index, right_index] = GetSplitIndices(split_id);

  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    ui::SimpleMenuModel* submenu = GetMuteSitesSubmenu(menu_model);
    ASSERT_TRUE(submenu);
    submenu->ActivatedAt(
        GetMuteSubmenuIndex(submenu, CommandId::kToggleStartSiteMuted));
  }
  EXPECT_TRUE(IsMuted(left_index));
  EXPECT_TRUE(IsMuted(right_index));

  // Both views now offer to unmute, and unmuting either of them unmutes both.
  {
    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, left_index);
    ui::SimpleMenuModel* submenu = GetMuteSitesSubmenu(menu_model);
    ASSERT_TRUE(submenu);
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleStartSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_UNMUTE_LEFT_SITE));
    EXPECT_EQ(GetMuteSubmenuLabel(submenu, CommandId::kToggleEndSiteMuted),
              l10n_util::GetStringUTF16(IDS_SPLIT_TAB_UNMUTE_RIGHT_SITE));
    submenu->ActivatedAt(
        GetMuteSubmenuIndex(submenu, CommandId::kToggleEndSiteMuted));
  }
  EXPECT_FALSE(IsMuted(left_index));
  EXPECT_FALSE(IsMuted(right_index));
}

class TabMenuModelSplitViewHorizontalBrowserTest
    : public TabMenuModelBrowserTest {
 public:
  TabMenuModelSplitViewHorizontalBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(tabs::kSplitViewHorizontal);
  }

  ui::SimpleMenuModel* GetArrangeSplitSubmenu(
      int tab_index,
      std::unique_ptr<TabMenuModel>& out_menu_model) {
    out_menu_model = std::make_unique<TabMenuModel>(
        &delegate_, TabMenuModelDelegate::From(browser()),
        browser()->tab_strip_model(), tab_index);
    size_t arrange_submenu_index =
        out_menu_model->GetIndexOfCommandId(TabStripModel::CommandArrangeSplit)
            .value();
    return static_cast<ui::SimpleMenuModel*>(
        out_menu_model->GetSubmenuModelAt(arrange_submenu_index));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelSplitViewHorizontalBrowserTest,
                       ToggleOrientationMenuAction) {
  chrome::NewTab(browser(), NewTabTypes::kNewTabCommand);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Creates a side-by-side split.
  tab_strip_model->AddToNewSplit(
      {0},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kSideBySide),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  auto split_id = tab_strip_model->GetSplitForTab(1);
  ASSERT_TRUE(split_id.has_value());
  EXPECT_EQ(tab_strip_model->GetSplitData(split_id.value())
                ->visual_data()
                ->split_layout(),
            split_tabs::SplitTabLayout::kSideBySide);

  // Verifies the context menu when the split view is side by side.
  std::unique_ptr<TabMenuModel> menu_model;
  ui::SimpleMenuModel* arrange_submenu = GetArrangeSplitSubmenu(1, menu_model);

  int toggle_cmd_id =
      ExistingBaseSubMenuModel::kMinSplitTabMenuModelCommandId +
      static_cast<int>(SplitTabMenuModel::CommandId::kToggleOrientation);

  size_t item_idx = arrange_submenu->GetIndexOfCommandId(toggle_cmd_id).value();
  EXPECT_EQ(arrange_submenu->GetLabelAt(item_idx),
            l10n_util::GetStringUTF16(IDS_SPLIT_TAB_SHOW_STACKED));

  // Toggle split view orientation.
  arrange_submenu->ActivatedAt(item_idx);
  EXPECT_EQ(tab_strip_model->GetSplitData(split_id.value())
                ->visual_data()
                ->split_layout(),
            split_tabs::SplitTabLayout::kStacked);

  // Verifies the context menu when the split view is stacked.
  std::unique_ptr<TabMenuModel> menu_model_2;
  ui::SimpleMenuModel* arrange_submenu_2 =
      GetArrangeSplitSubmenu(1, menu_model_2);

  size_t item_idx_2 =
      arrange_submenu_2->GetIndexOfCommandId(toggle_cmd_id).value();
  EXPECT_EQ(arrange_submenu_2->GetLabelAt(item_idx_2),
            l10n_util::GetStringUTF16(IDS_SPLIT_TAB_SHOW_SIDE_BY_SIDE));
}

class TabMenuModelSplitViewHorizontalDirectAccessBrowserTest
    : public TabMenuModelBrowserTest {
 public:
  TabMenuModelSplitViewHorizontalDirectAccessBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/GetEnabledFeaturesAndParams(),
        /*disabled_features=*/{});
  }

  virtual std::vector<base::test::FeatureRefAndParams>
  GetEnabledFeaturesAndParams() {
    return {{tabs::kSplitViewHorizontal,
             {{"split_view_horizontal_direct_access", "true"}}}};
  }

  void TestNewSplit(SplitViewLayoutMenuModel::CommandId command_id,
                    split_tabs::SplitTabLayout expected_layout) {
    chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    ASSERT_EQ(tab_strip_model->count(), 2);
    ASSERT_EQ(tab_strip_model->active_index(), 1);

    TabMenuModel menu_model(&delegate_, TabMenuModelDelegate::From(browser()),
                            tab_strip_model, 0);

    size_t submenu_index =
        menu_model.GetIndexOfCommandId(TabStripModel::CommandAddToSplit)
            .value();
    ui::SimpleMenuModel* submenu = static_cast<ui::SimpleMenuModel*>(
        menu_model.GetSubmenuModelAt(submenu_index));
    submenu->ActivatedAt(static_cast<size_t>(
        submenu->GetIndexOfCommandId(static_cast<int>(command_id)).value()));

    EXPECT_TRUE(tab_strip_model->GetActiveTab()->GetSplit().has_value());
    EXPECT_EQ(
        expected_layout,
        tab_strip_model
            ->GetSplitData(tab_strip_model->GetActiveTab()->GetSplit().value())
            ->visual_data()
            ->split_layout());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelSplitViewHorizontalDirectAccessBrowserTest,
                       NewSideBySideSplit) {
  TestNewSplit(SplitViewLayoutMenuModel::CommandId::kSideBySide,
               split_tabs::SplitTabLayout::kSideBySide);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelSplitViewHorizontalDirectAccessBrowserTest,
                       NewStackedSplit) {
  TestNewSplit(SplitViewLayoutMenuModel::CommandId::kStacked,
               split_tabs::SplitTabLayout::kStacked);
}

class TabMenuModelSplitViewHorizontalDirectTabAccessBrowserTest
    : public TabMenuModelSplitViewHorizontalDirectAccessBrowserTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeaturesAndParams()
      override {
    return {{tabs::kSplitViewHorizontal,
             {{"split_view_horizontal_direct_tab_access", "true"}}}};
  }
};

IN_PROC_BROWSER_TEST_F(
    TabMenuModelSplitViewHorizontalDirectTabAccessBrowserTest,
    NewSideBySideSplit) {
  TestNewSplit(SplitViewLayoutMenuModel::CommandId::kSideBySide,
               split_tabs::SplitTabLayout::kSideBySide);
}

IN_PROC_BROWSER_TEST_F(
    TabMenuModelSplitViewHorizontalDirectTabAccessBrowserTest,
    NewStackedSplit) {
  TestNewSplit(SplitViewLayoutMenuModel::CommandId::kStacked,
               split_tabs::SplitTabLayout::kStacked);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, SwapWithActiveTab) {
  // Add 3 tabs to the browser.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 3);

  // Create a split. Assert that the last two tabs are split with the rightmost
  // tab active.
  tab_strip_model->ActivateTabAt(0);
  tab_strip_model->AddToNewSplit(
      {1},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kSideBySide),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(0).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(1).has_value());
  EXPECT_FALSE(tab_strip_model->GetSplitForTab(2).has_value());
  EXPECT_EQ(tab_strip_model->active_index(), 0);

  // Trigger the swap start tab command.
  ActivateSwapWithSplitSubmenuCommand(
      2, SplitTabSwapMenuModel::CommandId::kSwapStartTab);

  // Check that now the left two tabs are in a split and the left (swapped) tab
  // in the split is active.
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(0).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(1).has_value());
  EXPECT_FALSE(tab_strip_model->GetSplitForTab(2).has_value());
  EXPECT_EQ(tab_strip_model->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, SwapWithInactiveTab) {
  // Add 3 tabs to the browser.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 3);

  // Create a split. Assert that the last two tabs are split with the rightmost
  // tab active.
  tab_strip_model->AddToNewSplit(
      {1},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kSideBySide),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  EXPECT_FALSE(tab_strip_model->GetSplitForTab(0).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(1).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(2).has_value());
  EXPECT_EQ(tab_strip_model->active_index(), 2);

  // Trigger the swap start tab command.
  ActivateSwapWithSplitSubmenuCommand(
      0, SplitTabSwapMenuModel::CommandId::kSwapStartTab);

  // Check that now the right two tabs are in a split and the right (swapped)
  // tab in the split is active.
  EXPECT_FALSE(tab_strip_model->GetSplitForTab(0).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(1).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(2).has_value());
  EXPECT_EQ(tab_strip_model->active_index(), 2);
}

IN_PROC_BROWSER_TEST_F(TabMenuModelBrowserTest, SwapWithSplitActiveTabChanged) {
  // Add 3 tabs to the browser.
  chrome::NewTab(browser(), NewTabTypes::kNewTabCommand);
  chrome::NewTab(browser(), NewTabTypes::kNewTabCommand);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 3);

  // Create a split with tabs 0 and 1. Tab 0 is active.
  tab_strip_model->ActivateTabAt(0);
  tab_strip_model->AddToNewSplit(
      {1},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kSideBySide),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(0).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(1).has_value());
  EXPECT_FALSE(tab_strip_model->GetSplitForTab(2).has_value());
  EXPECT_EQ(tab_strip_model->active_index(), 0);

  // Create the TabMenuModel for tab 2. This instantiates SplitTabSwapMenuModel.
  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    tab_strip_model, 2);
  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandSwapWithActiveSplit)
          .value();
  ui::SimpleMenuModel* submenu =
      static_cast<ui::SimpleMenuModel*>(menu.GetSubmenuModelAt(submenu_index));

  // Now, activate tab 2 (non-split tab) to simulate focus change.
  tab_strip_model->ActivateTabAt(2);
  EXPECT_FALSE(tab_strip_model->GetActiveTab()->IsSplit());

  // Trigger the swap start tab command from the submenu.
  // It should not crash and should be a no-op because the active tab is no
  // longer split.
  submenu->ActivatedAt(static_cast<size_t>(
      submenu
          ->GetIndexOfCommandId(
              static_cast<int>(SplitTabSwapMenuModel::CommandId::kSwapStartTab))
          .value()));

  // Verify that the split state did not change (tabs 0 and 1 are still split,
  // tab 2 is not).
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(0).has_value());
  EXPECT_TRUE(tab_strip_model->GetSplitForTab(1).has_value());
  EXPECT_FALSE(tab_strip_model->GetSplitForTab(2).has_value());
}

class TabMenuModelGlicMultiTabTest : public TabMenuModelBrowserTest {
 public:
  TabMenuModelGlicMultiTabTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{glic::mojom::features::kGlicMultiTab},
        /*disabled_features=*/{});
  }

 protected:
  TabStripModel* tab_strip() { return browser()->tab_strip_model(); }

  glic::GlicSharingManagerInternal& sharing_manager() {
    return glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile())
        ->active_instance_sharing_manager();
  }

  glic::GlicSharingManagerInternal& sharing_manager(
      glic::GlicInstance* instance) {
    CHECK(instance);
    return *static_cast<glic::GlicSharingManagerInternal*>(
        instance->GetSharingManager());
  }

  tabs::TabHandle TabHandleAtIndex(int index) {
    return tab_strip()->GetTabAtIndex(index)->GetHandle();
  }

  glic::GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelGlicMultiTabTest, NotShared) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabMenuModel model(&delegate_, TabMenuModelDelegate::From(browser()),
                     tab_strip_model, 1);
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandGlicShare).has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelGlicMultiTabTest, SomeShared) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  auto* service = glic::GlicKeyedService::Get(profile());
  service->ShowUI(browser(), glic::mojom::InvocationSource::kOsButtonMenu);
  auto* instance =
      service->GetInstanceForTab(browser()->GetActiveTabInterface());
  ASSERT_TRUE(instance);
  sharing_manager(instance).PinTabs(
      {tab_strip()->GetTabAtIndex(0)->GetHandle()},
      glic::GlicPinTrigger::kContextMenu);

  TabMenuModel model(&delegate_, TabMenuModelDelegate::From(browser()),
                     tab_strip(), 1);
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandGlicShare).has_value());
  EXPECT_FALSE(
      model.GetIndexOfCommandId(TabStripModel::CommandGlicUnshare).has_value());
}

IN_PROC_BROWSER_TEST_F(TabMenuModelGlicMultiTabTest, TooManyShared) {
  auto* service = glic::GlicKeyedService::Get(profile());
  service->ShowUI(browser(), glic::mojom::InvocationSource::kOsButtonMenu);
  auto* instance =
      service->GetInstanceForTab(browser()->GetActiveTabInterface());
  ASSERT_TRUE(instance);
  const int limit = sharing_manager(instance).GetMaxPinnedTabs();
  for (int i = 0; i < limit; ++i) {
    chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
    sharing_manager(instance).PinTabs(
        {tab_strip()->GetTabAtIndex(i)->GetHandle()},
        glic::GlicPinTrigger::kContextMenu);
  }
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  tab_strip()->SelectTabAt(limit);
  sharing_manager(instance).PinTabs(
      {tab_strip()->GetTabAtIndex(limit)->GetHandle()},
      glic::GlicPinTrigger::kContextMenu);
  EXPECT_FALSE(sharing_manager(instance).IsTabPinned(TabHandleAtIndex(limit)));
}

class TabMenuModelSendTabToSelfBrowserTest : public TabMenuModelBrowserTest {
 public:
  TabMenuModelSendTabToSelfBrowserTest() {
    feature_list_.InitAndEnableFeature(
        send_tab_to_self::kSendTabToSelfEnhancedDesktopUI);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    TabMenuModelBrowserTest::SetUpBrowserContextKeyedServices(context);
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              send_tab_to_self::StubSendTabToSelfSyncService>();
        }));
  }

  send_tab_to_self::FakeSendTabToSelfModel* model() {
    return static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile()))
        ->GetFakeSendTabToSelfModel();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabMenuModelSendTabToSelfBrowserTest,
                       SendMultipleSelectedTabs) {
  std::vector<send_tab_to_self::TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop,
                       syncer::DeviceInfo::OsType::kLinux, base::Time::Now());
  model()->SetTargetDeviceInfoSortedList(devices);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("/title3.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(2, url3, ui::PAGE_TRANSITION_TYPED));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip->count(), 3);

  // Multi-select tabs 1 and 2
  tab_strip->ActivateTabAt(1);
  tab_strip->AddSelectionFromAnchorTo(2);

  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    tab_strip, 2);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandSendTabToSelf).value();
  ui::SimpleMenuModel* submenu =
      static_cast<ui::SimpleMenuModel*>(menu.GetSubmenuModelAt(submenu_index));

  // Trigger send to device 0.
  submenu->ActivatedAt(0);

  // Wait for async SendTabToDevice requests (scroll position generation) to
  // complete.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return model()->GetAllGuids().size() == 2u; }));

  std::vector<GURL> sent_urls;
  for (const std::string& guid : model()->GetAllGuids()) {
    sent_urls.push_back(model()->GetEntryByGUID(guid)->GetURL());
  }
  EXPECT_THAT(sent_urls, testing::UnorderedElementsAre(url2, url3));
}

class TabMenuModelSendTabToSelfSigninPromosBrowserTest
    : public TabMenuModelSendTabToSelfBrowserTest {
 public:
  TabMenuModelSendTabToSelfSigninPromosBrowserTest() {
    signin_promos_feature_list_.InitWithFeatures(
        {send_tab_to_self::kSendTabToSelfSubmenuSigninPromos,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
         send_tab_to_self::kSendTabToSelfNoTargetDeviceQrCode
#endif
        },
        {});
  }

 private:
  base::test::ScopedFeatureList signin_promos_feature_list_;
};

// Tests that for `kOfferSignIn`, the tab context menu renders Send Tab to Self
// as a submenu and that activating the submenu item opens the promo bubble.
IN_PROC_BROWSER_TEST_F(TabMenuModelSendTabToSelfSigninPromosBrowserTest,
                       SubmenuForOfferSignIn) {
  auto* sync_service =
      static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  sync_service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kOfferSignIn);

  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    tab_strip, 0);
  std::optional<size_t> index =
      menu.GetIndexOfCommandId(TabStripModel::CommandSendTabToSelf);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(menu.GetTypeAt(*index), ui::MenuModel::TYPE_SUBMENU);

  ui::SimpleMenuModel* submenu =
      static_cast<ui::SimpleMenuModel*>(menu.GetSubmenuModelAt(*index));
  ASSERT_NE(submenu, nullptr);
  ASSERT_EQ(submenu->GetItemCount(), 2u);
  EXPECT_EQ(submenu->GetTypeAt(0), ui::MenuModel::TYPE_TITLE);
  EXPECT_EQ(submenu->GetCommandIdAt(1),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN);

  submenu->ActivatedAt(1);
  EXPECT_TRUE(send_tab_to_self::SendTabToSelfBubbleController::
                  GetOrCreateForWebContents(tab_strip->GetActiveWebContents())
                      ->IsBubbleShown());
}

// Tests that for `kOfferReauth`, the tab context menu renders Send Tab to Self
// as a submenu.
IN_PROC_BROWSER_TEST_F(TabMenuModelSendTabToSelfSigninPromosBrowserTest,
                       SubmenuForOfferReauth) {
  auto* sync_service =
      static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  sync_service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kOfferReauth);

  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    browser()->tab_strip_model(), 0);
  std::optional<size_t> index =
      menu.GetIndexOfCommandId(TabStripModel::CommandSendTabToSelf);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(menu.GetTypeAt(*index), ui::MenuModel::TYPE_SUBMENU);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests that for `kInformNoTargetDevice`, the tab context menu renders Send Tab
// to Self as a submenu when `kSendTabToSelfNoTargetDeviceQrCode` is enabled.
IN_PROC_BROWSER_TEST_F(TabMenuModelSendTabToSelfSigninPromosBrowserTest,
                       SubmenuForInformNoTargetDevice) {
  auto* sync_service =
      static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  sync_service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice);

  TabMenuModel menu(&delegate_, TabMenuModelDelegate::From(browser()),
                    browser()->tab_strip_model(), 0);
  std::optional<size_t> index =
      menu.GetIndexOfCommandId(TabStripModel::CommandSendTabToSelf);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(menu.GetTypeAt(*index), ui::MenuModel::TYPE_SUBMENU);
}
#endif
