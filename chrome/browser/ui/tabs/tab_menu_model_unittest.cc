// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include <set>

#include "base/observer_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/contextual_tasks_tab_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#endif

namespace {

class FakeActiveTaskContextProvider
    : public contextual_tasks::ActiveTaskContextProvider {
 public:
  explicit FakeActiveTaskContextProvider(BrowserWindowInterface* window)
      : scoped_data_(window->GetUnownedUserDataHost(), *this) {}
  ~FakeActiveTaskContextProvider() override {
    for (auto& observer : observers_) {
      observer.OnActiveTaskContextProviderDestroyed();
    }
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }
  void RefreshContext() override {}
  void SetContextualTasksPanelController(
      contextual_tasks::ContextualTasksPanelController*) override {}
  void AddLocalTabUnderline(tabs::TabHandle) override {}
  void RemoveLocalTabUnderline(tabs::TabHandle) override {}
  void ClearAllLocalTabUnderlines() override {}

  void NotifyContextTabsChanged(const std::set<tabs::TabHandle>& context_tabs) {
    for (auto& observer : observers_) {
      observer.OnContextTabsChanged(context_tabs);
    }
  }

 private:
  base::ObserverList<Observer> observers_;
  ui::ScopedUnownedUserData<contextual_tasks::ActiveTaskContextProvider>
      scoped_data_;
};

}  // namespace

// A TabStripModelDelegate for simulating web apps.
class WebAppTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  bool IsForWebApp() override { return true; }

  bool SupportsReadLater() override { return false; }
};

// A TabMenuModelDelegate that simulates no other browser windows being open.
class TabMenuModelTestDelegate : public TabMenuModelDelegate {
 public:
  explicit TabMenuModelTestDelegate(ui::UnownedUserDataHost& host)
      : TabMenuModelDelegate(host) {}

  std::vector<BrowserWindowInterface*> GetOtherBrowserWindows(
      bool is_app) override {
    return {};
  }
  tab_groups::TabGroupSyncService* GetTabGroupSyncService() override {
    return nullptr;
  }
};

class TabMenuModelTest : public MenuModelTest, public ::testing::Test {
 public:
  TabMenuModelTest() = default;
  ~TabMenuModelTest() override = default;

  Profile* profile() { return &profile_; }

  TabMenuModelDelegate& menu_model_delegate() { return menu_model_delegate_; }

 private:
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  TabMenuModelTestDelegate menu_model_delegate_{unowned_user_data_host_};
};

TEST_F(TabMenuModelTest, TabbedWebApp) {
  // Create a tabbed web app window without home tab.
  WebAppTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      true);

  TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(model.GetItemCount(), 7u);

  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandCopyURL).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandReload).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandGoBack).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());

  EXPECT_EQ(model.GetTypeAt(4), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandCloseTab).has_value());
  EXPECT_TRUE(model.GetIndexOfCommandId(TabStripModel::CommandCloseOtherTabs)
                  .has_value());
}

TEST_F(TabMenuModelTest, TabbedWebAppHomeTab) {
  WebAppTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      true);

  // Pin the first tab so we get the pinned home tab menu.
  tab_strip_model.SetTabPinned(0, true);

  TabMenuModel home_tab_model(&delegate_, &menu_model_delegate(),
                              &tab_strip_model, 0);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(home_tab_model.GetItemCount(), 5u);

  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandCopyURL)
                  .has_value());
  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandReload)
                  .has_value());
  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandGoBack)
                  .has_value());

  EXPECT_EQ(home_tab_model.GetTypeAt(3), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      home_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseAllTabs)
          .has_value());

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      true);
  EXPECT_EQ(tab_strip_model.count(), 2);
  EXPECT_FALSE(tab_strip_model.IsTabSelected(0));
  EXPECT_TRUE(tab_strip_model.IsTabSelected(1));

  TabMenuModel regular_tab_model(&delegate_, &menu_model_delegate(),
                                 &tab_strip_model, 1);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(regular_tab_model.GetItemCount(), 8u);

  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCopyURL)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandReload)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandGoBack)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model
          .GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());

  EXPECT_EQ(regular_tab_model.GetTypeAt(4), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseTab)
          .has_value());
  EXPECT_TRUE(regular_tab_model
                  .GetIndexOfCommandId(TabStripModel::CommandCloseOtherTabs)
                  .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseAllTabs)
          .has_value());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(TabMenuModelTest, ExtensionItems) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kExtensionTabContextMenu);

  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());

  // Initialize MenuManager for the TestingProfile.
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      /*foreground=*/true);

  // Create a mock extension.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").Build();
  ASSERT_TRUE(extension);
  extensions::ExtensionRegistry::Get(profile())->AddEnabled(extension);

  // Create a baseline model to capture the number of standard items.
  TabMenuModel baseline_model(&delegate_, &menu_model_delegate(),
                              &tab_strip_model, 0);
  size_t baseline_count = baseline_model.GetItemCount();

  // Add a context menu item with 'tab' layout context to MenuManager.
  extensions::MenuManager* manager = extensions::MenuManager::Get(profile());
  ASSERT_TRUE(manager);
  extensions::MenuItem::Id id(
      profile()->IsOffTheRecord(),
      extensions::MenuItem::ExtensionKey(extension->id()));
  auto item = std::make_unique<extensions::MenuItem>(
      id, "Test Extension Item", /*checked=*/false, /*visible=*/true,
      /*enabled=*/true, extensions::MenuItem::NORMAL,
      extensions::MenuItem::ContextList(extensions::MenuItem::TAB));

  // Use correct AddContextItem method.
  manager->AddContextItem(extension.get(), std::move(item));

  TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);

  // Verify that the menu model successfully populated items (standard +
  // extension).
  size_t count = model.GetItemCount();
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, ContextMenuMatcher does not add a separator before the first
  // extension item. See:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/context_menu_matcher.cc;l=87-97;drc=8b9a95f24181fb0d3975b1760045e0d1ed38c159
  EXPECT_EQ(count, baseline_count + 1);  // Adds 1 item (no separator).
#else
  EXPECT_EQ(count, baseline_count + 2);  // Adds 1 separator and 1 item.
#endif

  // Find the index of the extension item by searching for an extension custom
  // command ID.
  size_t ext_index = count;
  for (size_t i = 0; i < count; ++i) {
    if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
            model.GetCommandIdAt(i))) {
      ext_index = i;
      break;
    }
  }
  ASSERT_LT(ext_index, count);

  // Cast to base class to access private overrides
  ui::MenuModel* menu_base = &model;
  EXPECT_TRUE(menu_base->IsVisibleAt(ext_index));
  EXPECT_TRUE(menu_base->IsEnabledAt(ext_index));
  EXPECT_FALSE(menu_base->IsItemCheckedAt(ext_index));
}
#endif

TEST_F(TabMenuModelTest, EphemeralGroupOmitsRemoveFromGroup) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Initialize MenuManager for the TestingProfile.
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      true);

  // When tab 0 is in a standard group, CommandRemoveFromGroup is present.
  tab_strip_model.AddToNewGroup({0});
  TabMenuModel standard_group_menu(&delegate_, &menu_model_delegate(),
                                   &tab_strip_model, 0);
  EXPECT_TRUE(standard_group_menu
                  .GetIndexOfCommandId(TabStripModel::CommandRemoveFromGroup)
                  .has_value());

  // Remove tab 0 from the standard group.
  tab_strip_model.RemoveFromGroup({0});

  // When tab 0 is in an ephemeral group, CommandRemoveFromGroup is omitted.
  tab_strip_model.AddToNewGroup({0}, /*is_ephemeral=*/true);
  TabMenuModel ephemeral_group_menu(&delegate_, &menu_model_delegate(),
                                    &tab_strip_model, 0);
  EXPECT_FALSE(ephemeral_group_menu
                   .GetIndexOfCommandId(TabStripModel::CommandRemoveFromGroup)
                   .has_value());
}

TEST_F(TabMenuModelTest, FocusSelectionMenuItem) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kTabGroupsFocusing, features::kNonGroupFocus}, {});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  for (int i = 0; i < 4; ++i) {
    tab_strip_model.AppendWebContents(
        content::WebContents::Create(
            content::WebContents::CreateParams(profile())),
        i == 0);
  }

  // Verifying "Focus on Tab" label.
  {
    ui::ListSelectionModel selection;
    selection.SetSelectedIndex(0);
    tab_strip_model.SetSelectionFromModel(selection);
    TabMenuModel menu(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);
    std::optional<size_t> index =
        menu.GetIndexOfCommandId(TabStripModel::CommandToggleFocusGroup);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(menu.GetLabelAt(*index),
              l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_FOCUS_TABS, 1));
  }

  // Verifying "Focus on Tabs" label.
  {
    ui::ListSelectionModel selection;
    selection.SetSelectedIndex(0);
    selection.AddIndexToSelection(1);
    tab_strip_model.SetSelectionFromModel(selection);
    TabMenuModel menu(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);
    std::optional<size_t> index =
        menu.GetIndexOfCommandId(TabStripModel::CommandToggleFocusGroup);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(menu.GetLabelAt(*index),
              l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_FOCUS_TABS, 2));
  }

  // Verifying "Focus on Tab Group" and "Unfocus Tab Group" labels.
  tab_groups::TabGroupId group1 = tab_strip_model.AddToNewGroup({0, 1});
  {
    ui::ListSelectionModel selection;
    selection.SetSelectedIndex(0);
    tab_strip_model.SetSelectionFromModel(selection);
    TabMenuModel single_menu(&delegate_, &menu_model_delegate(),
                             &tab_strip_model, 0);
    std::optional<size_t> single_idx =
        single_menu.GetIndexOfCommandId(TabStripModel::CommandToggleFocusGroup);
    ASSERT_TRUE(single_idx.has_value());
    EXPECT_EQ(single_menu.GetLabelAt(*single_idx),
              l10n_util::GetStringUTF16(IDS_TAB_CXMENU_FOCUS_TAB_GROUP));

    selection.AddIndexToSelection(1);
    tab_strip_model.SetSelectionFromModel(selection);
    TabMenuModel multi_menu(&delegate_, &menu_model_delegate(),
                            &tab_strip_model, 0);
    std::optional<size_t> multi_idx =
        multi_menu.GetIndexOfCommandId(TabStripModel::CommandToggleFocusGroup);
    ASSERT_TRUE(multi_idx.has_value());
    EXPECT_EQ(multi_menu.GetLabelAt(*multi_idx),
              l10n_util::GetStringUTF16(IDS_TAB_CXMENU_FOCUS_TAB_GROUP));

    tab_strip_model.EnterFocusMode(group1);
    TabMenuModel focused_menu(&delegate_, &menu_model_delegate(),
                              &tab_strip_model, 0);
    std::optional<size_t> focused_idx = focused_menu.GetIndexOfCommandId(
        TabStripModel::CommandToggleFocusGroup);
    ASSERT_TRUE(focused_idx.has_value());
    EXPECT_EQ(focused_menu.GetLabelAt(*focused_idx),
              l10n_util::GetStringUTF16(IDS_TAB_CXMENU_UNFOCUS_TAB_GROUP));
    tab_strip_model.ExitFocusMode();
  }
}

TEST_F(TabMenuModelTest, AiModeShareSubMenuEnabledForValidUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       contextual_tasks::kContextualTasksForceEntryPointEligibility,
       contextual_tasks::kContextualTasksSidePanelRearchitecture},
      {});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  ui::UnownedUserDataHost host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(host));
  contextual_tasks::MockContextualTasksPanelController mock_panel_controller(
      &mock_browser_window);

  TestTabStripModelDelegate delegate;
  delegate.SetBrowserWindowInterface(&mock_browser_window);
  TabStripModel tab_strip_model(&delegate, profile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  tab_strip_model.AppendWebContents(std::move(web_contents), true);

  TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);
  std::optional<size_t> share_index =
      model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare);
  ASSERT_TRUE(share_index.has_value());
  EXPECT_TRUE(model.IsEnabledAt(*share_index));
  EXPECT_EQ(model.GetTypeAt(*share_index), ui::MenuModel::TYPE_SUBMENU);
  EXPECT_EQ(model.GetLabelAt(*share_index),
            l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_AI_MODE_SHARE, 1));
  EXPECT_FALSE(model.GetIconAt(*share_index).IsEmpty());

  ui::MenuModel* submenu = model.GetSubmenuModelAt(*share_index);
  ASSERT_TRUE(submenu);
  ASSERT_EQ(submenu->GetItemCount(), 1u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            TabStripModel::CommandAiModeCreateNewThread);
  EXPECT_TRUE(submenu->IsEnabledAt(0));

  EXPECT_CALL(mock_panel_controller,
              ShareTabsWithActiveOrNewThread(testing::ElementsAre(
                  tab_strip_model.GetTabAtIndex(0)->GetHandle())))
      .Times(1);
  submenu->ActivatedAt(0);
}

TEST_F(TabMenuModelTest, AiModeShareMultiTabPluralization) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       contextual_tasks::kContextualTasksForceEntryPointEligibility,
       contextual_tasks::kContextualTasksSidePanelRearchitecture},
      {});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL("https://example.com"));
    tab_strip_model.AppendWebContents(std::move(web_contents), true);
  }
  tab_strip_model.SelectTabAt(0);

  TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);
  std::optional<size_t> share_index =
      model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare);
  ASSERT_TRUE(share_index.has_value());
  EXPECT_TRUE(model.IsEnabledAt(*share_index));
  EXPECT_EQ(model.GetLabelAt(*share_index),
            l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_AI_MODE_SHARE, 2));
}

TEST_F(TabMenuModelTest, AiModeShareHiddenForInvalidUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       contextual_tasks::kContextualTasksForceEntryPointEligibility,
       contextual_tasks::kContextualTasksSidePanelRearchitecture},
      {});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  for (const char* invalid_url :
       {"about:blank", "chrome://version", "chrome-untrusted://test",
        "chrome-extension://abcdefghijklmnop/index.html"}) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL(invalid_url));
    tab_strip_model.AppendWebContents(std::move(web_contents), true);

    TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model,
                       tab_strip_model.count() - 1);
    EXPECT_FALSE(model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
                     .has_value())
        << "Expected CommandAiModeShare to be hidden for URL: " << invalid_url;
  }
}

TEST_F(TabMenuModelTest, AiModeShareHiddenWhenPolicyDisabledOrOffTheRecord) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       contextual_tasks::kContextualTasksForceEntryPointEligibility,
       contextual_tasks::kContextualTasksSidePanelRearchitecture},
      {});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  tab_strip_model.AppendWebContents(std::move(web_contents), true);

  TabMenuModel disabled_pref_model(&delegate_, &menu_model_delegate(),
                                   &tab_strip_model, 0);
  EXPECT_FALSE(
      disabled_pref_model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
          .has_value());

  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      otr_profile, base::BindOnce([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif
  TabStripModel otr_tab_strip_model(&delegate, otr_profile);
  std::unique_ptr<content::WebContents> otr_web_contents =
      content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);
  content::WebContentsTester::For(otr_web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  otr_tab_strip_model.AppendWebContents(std::move(otr_web_contents), true);

  TabMenuModel otr_model(&delegate_, &menu_model_delegate(),
                         &otr_tab_strip_model, 0);
  EXPECT_FALSE(otr_model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
                   .has_value());
}

TEST_F(TabMenuModelTest, AiModeShareSubMenuRecentThreads) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       contextual_tasks::kContextualTasksForceEntryPointEligibility,
       contextual_tasks::kContextualTasksSidePanelRearchitecture},
      {});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  ui::UnownedUserDataHost host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(host));
  contextual_tasks::MockContextualTasksPanelController mock_panel_controller(
      &mock_browser_window);

  base::Uuid thread_id = base::Uuid::GenerateRandomV4();
  std::vector<
      contextual_tasks::ContextualTasksPanelController::RecentThreadInfo>
      recent_threads = {
          {thread_id, u"Recent Thread 1"},
      };
  ON_CALL(mock_panel_controller, GetRecentThreads())
      .WillByDefault(testing::Return(recent_threads));

  TestTabStripModelDelegate delegate;
  delegate.SetBrowserWindowInterface(&mock_browser_window);
  TabStripModel tab_strip_model(&delegate, profile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  tab_strip_model.AppendWebContents(std::move(web_contents), true);

  TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);
  std::optional<size_t> share_index =
      model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare);
  ASSERT_TRUE(share_index.has_value());

  ui::MenuModel* submenu = model.GetSubmenuModelAt(*share_index);
  ASSERT_TRUE(submenu);
  ASSERT_EQ(submenu->GetItemCount(), 3u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            TabStripModel::CommandAiModeCreateNewThread);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetCommandIdAt(2),
            contextual_tasks::ContextualTasksTabSubMenuModel::
                kMinRecentThreadCommandId);
  EXPECT_EQ(submenu->GetLabelAt(2), u"Recent Thread 1");
  EXPECT_TRUE(submenu->IsEnabledAt(2));

  // Verify root menu tree command ID dispatch resolves to the AI Mode submenu.
  ui::MenuModel* resolved_model = &model;
  size_t resolved_index = 0;
  ASSERT_TRUE(ui::MenuModel::GetModelAndIndexForCommandId(
      contextual_tasks::ContextualTasksTabSubMenuModel::
          kMinRecentThreadCommandId,
      &resolved_model, &resolved_index));
  EXPECT_EQ(resolved_model, submenu);
  EXPECT_EQ(resolved_index, 2u);

  EXPECT_CALL(
      mock_panel_controller,
      ShareTabsWithExistingThread(
          thread_id,
          testing::ElementsAre(tab_strip_model.GetTabAtIndex(0)->GetHandle()),
          omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT))
      .Times(1);
  resolved_model->ActivatedAt(resolved_index);

  tab_strip_model.CloseAllTabs();
  delegate.SetBrowserWindowInterface(nullptr);
}

TEST_F(TabMenuModelTest, AiModeUnshareShownWhenTabInContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{contextual_tasks::kContextualTasks, {}},
       {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
       {contextual_tasks::kContextualTasksSidePanelRearchitecture, {}},
       {omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "true"}}}},
      {});

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));
#endif

  ui::UnownedUserDataHost user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> browser_window;
  ON_CALL(browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));
  ON_CALL(browser_window, GetProfile())
      .WillByDefault(testing::Return(profile()));

  FakeActiveTaskContextProvider context_provider(&browser_window);
  testing::NiceMock<contextual_tasks::MockContextualTasksPanelController>
      mock_panel_controller(&browser_window);

  TestTabStripModelDelegate delegate;
  delegate.SetBrowserWindowInterface(&browser_window);
  TabStripModel tab_strip_model(&delegate, profile());

  std::unique_ptr<content::WebContents> wc1 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(wc1.get())->NavigateAndCommit(
      GURL("https://example.com/1"));
  tab_strip_model.AppendWebContents(std::move(wc1), /*foreground=*/true);

  std::unique_ptr<content::WebContents> wc2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(wc2.get())->NavigateAndCommit(
      GURL("https://example.com/2"));
  tab_strip_model.AppendWebContents(std::move(wc2), /*foreground=*/true);
  tab_strip_model.SelectTabAt(0);

  auto alert_controller1 = std::make_unique<tabs::TabAlertController>(
      *tab_strip_model.GetTabAtIndex(0));
  auto alert_controller2 = std::make_unique<tabs::TabAlertController>(
      *tab_strip_model.GetTabAtIndex(1));

  tabs::TabHandle tab_handle1 = tab_strip_model.GetTabAtIndex(0)->GetHandle();
  tabs::TabHandle tab_handle2 = tab_strip_model.GetTabAtIndex(1)->GetHandle();

  // When neither selected tab is in context, Unshare is disabled and hidden.
  context_provider.NotifyContextTabsChanged({});
  EXPECT_FALSE(tab_strip_model.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAiModeUnshare));
  {
    TabMenuModel unshared_model(&delegate_, &menu_model_delegate(),
                                &tab_strip_model, 0);
    EXPECT_TRUE(
        unshared_model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
            .has_value());
    EXPECT_FALSE(
        unshared_model.GetIndexOfCommandId(TabStripModel::CommandAiModeUnshare)
            .has_value());
  }

  // When at least one selected tab is in context, both Share and Unshare show
  // with the expected label and dispatch `UnshareTabs()` for all selected tabs.
  context_provider.NotifyContextTabsChanged({tab_handle2});
  EXPECT_TRUE(tab_strip_model.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAiModeUnshare));
  {
    TabMenuModel shared_model(&delegate_, &menu_model_delegate(),
                              &tab_strip_model, 0);
    EXPECT_TRUE(
        shared_model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
            .has_value());
    std::optional<size_t> unshare_index =
        shared_model.GetIndexOfCommandId(TabStripModel::CommandAiModeUnshare);
    ASSERT_TRUE(unshare_index.has_value());
    EXPECT_EQ(shared_model.GetLabelAt(*unshare_index),
              l10n_util::GetStringUTF16(IDS_TAB_CXMENU_AI_MODE_UNSHARE));
  }

  base::HistogramTester histogram_tester;
  EXPECT_CALL(mock_panel_controller,
              UnshareTabs(testing::ElementsAre(tab_handle1, tab_handle2)))
      .Times(1);
  tab_strip_model.ExecuteContextMenuCommand(
      0, TabStripModel::CommandAiModeUnshare);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.AiModeUnshare.SelectedTabsCount", 2, 1);

  // Disabling content sharing policy or tab deselection disables
  // `CommandAiModeUnshare`.
  {
    base::test::ScopedFeatureList deselection_disabled;
    deselection_disabled.InitAndDisableFeature(
        omnibox::kContextManagementInComposebox);
    EXPECT_FALSE(tab_strip_model.IsContextMenuCommandEnabled(
        0, TabStripModel::CommandAiModeUnshare));
  }
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));
  EXPECT_FALSE(tab_strip_model.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandAiModeUnshare));
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  // On an unselected internal chrome:// tab that is still in context (e.g.
  // attached prior to navigation or via Chromnient), Share is hidden while
  // Unshare still shows both with default menu layout and under
  // `features::kMenuSimplification`.
  std::unique_ptr<content::WebContents> wc3 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(wc3.get())->NavigateAndCommit(
      GURL("chrome://version"));
  tab_strip_model.AppendWebContents(std::move(wc3), /*foreground=*/false);
  auto alert_controller3 = std::make_unique<tabs::TabAlertController>(
      *tab_strip_model.GetTabAtIndex(2));
  tabs::TabHandle tab_handle3 = tab_strip_model.GetTabAtIndex(2)->GetHandle();
  context_provider.NotifyContextTabsChanged({tab_handle2, tab_handle3});
  {
    TabMenuModel internal_model(&delegate_, &menu_model_delegate(),
                                &tab_strip_model, 2);
    EXPECT_FALSE(
        internal_model.GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
            .has_value());
    EXPECT_TRUE(
        internal_model.GetIndexOfCommandId(TabStripModel::CommandAiModeUnshare)
            .has_value());
  }

  {
    base::test::ScopedFeatureList menu_simplification_feature;
    menu_simplification_feature.InitAndEnableFeature(
        features::kMenuSimplification);

    TabMenuModel simplified_internal_model(&delegate_, &menu_model_delegate(),
                                           &tab_strip_model, 2);
    EXPECT_FALSE(simplified_internal_model
                     .GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
                     .has_value());
    std::optional<size_t> simplified_unshare_index =
        simplified_internal_model.GetIndexOfCommandId(
            TabStripModel::CommandAiModeUnshare);
    ASSERT_TRUE(simplified_unshare_index.has_value());
    ASSERT_GT(*simplified_unshare_index, 0u);
    ASSERT_LT(*simplified_unshare_index + 1,
              simplified_internal_model.GetItemCount());
    EXPECT_EQ(
        simplified_internal_model.GetTypeAt(*simplified_unshare_index - 1),
        ui::MenuModel::TYPE_SEPARATOR);
    EXPECT_EQ(
        simplified_internal_model.GetTypeAt(*simplified_unshare_index + 1),
        ui::MenuModel::TYPE_SEPARATOR);

    TabMenuModel simplified_web_model(&delegate_, &menu_model_delegate(),
                                      &tab_strip_model, 0);
    EXPECT_TRUE(simplified_web_model
                    .GetIndexOfCommandId(TabStripModel::CommandAiModeShare)
                    .has_value());
  }

  alert_controller3.reset();
  alert_controller2.reset();
  alert_controller1.reset();
  tab_strip_model.CloseAllTabs();
  delegate.SetBrowserWindowInterface(nullptr);
}
