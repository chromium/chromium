// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_api.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/lens/lens_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"

using testing::_;

namespace {

// Creates a basic SidePanelEntry for the given `key` that returns an empty view
// when shown.
std::unique_ptr<SidePanelEntry> CreateEntry(const SidePanelEntry::Key& key) {
  return std::make_unique<SidePanelEntry>(
      key,
      base::BindRepeating([]() { return std::make_unique<views::View>(); }));
}

}  // namespace

class SidePanelCoordinatorTest : public InProcessBrowserTest {
 public:
  SidePanelCoordinatorTest() {
    scoped_feature_list_.InitWithFeatures({features::kSidePanelResizing}, {});
  }
  virtual void Init() {
    AddTabToBrowser(GURL("http://foo1.com"));
    AddTabToBrowser(GURL("http://foo2.com"));
    browser()->tab_strip_model()->ActivateTabAt(0);

    // Add some entries to the first tab.
    auto* registry = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->side_panel_registry();
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kShoppingInsights,
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));
    contextual_registries_.push_back(registry);

    // Add some entries to the second tab.
    browser()->tab_strip_model()->ActivateTabAt(1);
    registry = browser()
                   ->GetActiveTabInterface()
                   ->GetTabFeatures()
                   ->side_panel_registry();
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLens,
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));
    contextual_registries_.push_back(browser()
                                         ->GetActiveTabInterface()
                                         ->GetTabFeatures()
                                         ->side_panel_registry());

    // Add a kLensOverlayResults entry to the contextual registry for the second
    // tab.
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLensOverlayResults,
        base::BindRepeating([]() { return std::make_unique<views::View>(); }),
        std::nullopt, base::BindRepeating([]() {
          return std::unique_ptr<ui::MenuModel>(
              new ui::SimpleMenuModel(nullptr));
        })));
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kShoppingInsights,
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));

    coordinator()->SetNoDelaysForTesting(true);
  }

  void SetUpPinningTest() {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    auto* const registry = SidePanelRegistry::GetDeprecated(web_contents);
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kAboutThisSite,
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));
    contextual_registries_.push_back(registry);
  }

  void VerifyEntryExistenceAndValue(std::optional<SidePanelEntry*> entry,
                                    SidePanelEntry::Id id) {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value()->key().id(), id);
  }

  void VerifyEntryExistenceAndValue(std::optional<SidePanelEntry*> entry,
                                    const SidePanelEntry::Key& key) {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value()->key(), key);
  }

  void VerifyEntryExistenceAndValue(std::optional<SidePanelEntry::Id> entry,
                                    SidePanelEntry::Id id) {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value(), id);
  }

  const std::u16string& GetTitleText() {
    return coordinator()->panel_title_->GetText();
  }

  void AddTabToBrowser(const GURL& tab_url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), tab_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

 protected:
  void WaitForExtensionsContainerAnimation() {
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  }

  void ClickButton(views::Button* button) {
    views::test::ButtonTestApi(button).NotifyClick(ui::MouseEvent(
        ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  SidePanelEntry::Key GetKeyForExtension(const extensions::ExtensionId& id) {
    return SidePanelEntry::Key(SidePanelEntry::Id::kExtension, id);
  }

  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() const {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->extensions_container();
  }

  // Calls chrome.sidePanel.setOptions() for the given `extension`, `path` and
  // `enabled` and returns when the API call is complete.
  void RunSetOptions(const extensions::Extension& extension,
                     std::optional<int> tab_id,
                     std::optional<std::string> path,
                     bool enabled) {
    auto function =
        base::MakeRefCounted<extensions::SidePanelSetOptionsFunction>();
    function->set_extension(&extension);

    std::string tab_id_arg =
        tab_id.has_value() ? base::StringPrintf(R"("tabId":%d,)", *tab_id) : "";
    std::string path_arg =
        path.has_value() ? base::StringPrintf(R"("path":"%s",)", path->c_str())
                         : "";
    std::string args =
        base::StringPrintf(R"([{%s%s"enabled":%s}])", tab_id_arg.c_str(),
                           path_arg.c_str(), enabled ? "true" : "false");
    EXPECT_TRUE(extensions::api_test_utils::RunFunction(function.get(), args,
                                                        browser()->profile()))
        << function->GetError();
  }

  scoped_refptr<const extensions::Extension> LoadSidePanelExtension(
      const std::string& name) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .SetManifestVersion(3)
            .AddAPIPermission("sidePanel")
            .Build();

    extension_service()->GrantPermissions(extension.get());
    extension_service()->AddExtension(extension.get());

    return extension;
  }

  scoped_refptr<const extensions::Extension> AddExtensionWithSidePanel(
      const std::string& name,
      std::optional<int> tab_id) {
    scoped_refptr<const extensions::Extension> extension =
        LoadSidePanelExtension(name);
    // Set a global panel with the path to the side panel to use.
    RunSetOptions(*extension, tab_id,
                  /*path=*/"panel.html",
                  /*enabled=*/true);
    return extension;
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service();
  }

  SidePanelCoordinator* coordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }

  SidePanelRegistry* global_registry() {
    return coordinator()->GetWindowRegistry();
  }

  std::vector<raw_ptr<SidePanelRegistry, DanglingUntriaged>>
      contextual_registries_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MockSidePanelViewStateObserver : public SidePanelViewStateObserver {
 public:
  MOCK_METHOD(void, OnSidePanelDidClose, (), (override));
  MOCK_METHOD(void, OnSidePanelDidOpen, (), (override));
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ToggleSidePanel) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, OpenWhileClosing) {
  Init();
  // Wait for the side panel to be visible and fully shown.
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().unified_side_panel()->state() ==
           SidePanel::State::kOpen;
  }));

  // Closing the side panel is asynchronous.
  coordinator()->Close();
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->state(),
            SidePanel::State::kClosing);

  // Opening the same entry should cancel the close.
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  auto state = browser()->GetBrowserView().unified_side_panel()->state();
  EXPECT_TRUE(state == SidePanel::State::kOpen ||
              state == SidePanel::State::kOpening);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidth) {
  Init();
  // Set side panel to right-aligned
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  const int starting_width = 500;
  browser()->GetBrowserView().unified_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width);

  const int increment = 50;
  browser()->GetBrowserView().unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width - increment);

  // Set side panel to left-aligned
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  browser()->GetBrowserView().unified_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width);

  browser()->GetBrowserView().unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width + increment);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidthMaxMin) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  const int starting_width = 500;
  browser()->GetBrowserView().unified_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width);

  // Use an increment large enough to hit side panel and browser contents
  // minimum width constraints.
  const int large_increment = 1000000000;
  browser()->GetBrowserView().unified_side_panel()->OnResize(large_increment,
                                                             true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            browser()
                ->GetBrowserView()
                .unified_side_panel()
                ->GetMinimumSize()
                .width());

  browser()->GetBrowserView().unified_side_panel()->OnResize(-large_increment,
                                                             true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  BrowserViewLayout* layout_manager = static_cast<BrowserViewLayout*>(
      browser()->GetBrowserView().GetLayoutManager());
  const int min_web_contents_width =
      layout_manager->GetMinWebContentsWidthForTesting();
  EXPECT_EQ(browser()->GetBrowserView().contents_web_view()->width(),
            min_web_contents_width);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidthRTL) {
  Init();
  // Set side panel to right-aligned
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  // Set UI direction to LTR
  base::i18n::SetRTLForTesting(false);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  const int starting_width = 500;
  browser()->GetBrowserView().unified_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width);

  const int increment = 50;
  browser()->GetBrowserView().unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width - increment);

  // Set UI direction to RTL
  base::i18n::SetRTLForTesting(true);
  browser()->GetBrowserView().unified_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width);

  browser()->GetBrowserView().unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().unified_side_panel()->width(),
            starting_width + increment);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ChangeSidePanelWidthWindowResize) {
  Init();
  // Wait for the side panel to be visible and fully shown.
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().unified_side_panel()->state() ==
           SidePanel::State::kOpen;
  }));

  // Set the width and wait for layout/animations.
  const int starting_width = 500;
  browser()->GetBrowserView().unified_side_panel()->SetPanelWidth(
      starting_width);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().unified_side_panel()->width() ==
           starting_width;
  }));

  // Shrink browser window enough that side panel should also shrink in
  // observance of web contents minimum width.
  gfx::Rect original_bounds(browser()->GetBrowserView().GetBounds());
  gfx::Size new_size(starting_width, starting_width);
  gfx::Rect new_bounds(original_bounds);
  new_bounds.set_size(new_size);
  // Explicitly restore the browser window on ChromeOS, as it would otherwise
  // be maximized and the SetBounds call would be a no-op.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  browser()->GetBrowserView().Restore();
#endif
  browser()->GetBrowserView().SetBounds(new_bounds);
  BrowserViewLayout* layout_manager = static_cast<BrowserViewLayout*>(
      browser()->GetBrowserView().GetLayoutManager());
  const int min_web_contents_width =
      layout_manager->GetMinWebContentsWidthForTesting();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    // Within a couple of pixels.
    return browser()->GetBrowserView().contents_web_view()->width() <
           min_web_contents_width + 20;
  }));

  // Return browser window to original size, side panel should also return to
  // size prior to window resize.
  browser()->GetBrowserView().SetBounds(original_bounds);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().unified_side_panel()->width() ==
           starting_width;
  }));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelAlignment) {
  Init();
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(
      browser()->GetBrowserView().unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .unified_side_panel()
                ->GetHorizontalAlignment(),
            SidePanel::HorizontalAlignment::kRight);

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(
      browser()->GetBrowserView().unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .unified_side_panel()
                ->GetHorizontalAlignment(),
            SidePanel::HorizontalAlignment::kLeft);
}

// Verify that right and left alignment works the same as when in LTR mode.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelAlignmentRTL) {
  Init();
  // Forcing the language to hebrew causes the ui to enter RTL mode.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(
      browser()->GetBrowserView().unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .unified_side_panel()
                ->GetHorizontalAlignment(),
            SidePanel::HorizontalAlignment::kRight);

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(
      browser()->GetBrowserView().unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .unified_side_panel()
                ->GetHorizontalAlignment(),
            SidePanel::HorizontalAlignment::kLeft);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       DontNotifySidePanelObserverOfChangingContent) {
  Init();
  MockSidePanelViewStateObserver view_state_observer;
  EXPECT_CALL(view_state_observer, OnSidePanelDidOpen()).Times(1);
  EXPECT_CALL(view_state_observer, OnSidePanelDidClose()).Times(0);

  coordinator()->AddSidePanelViewStateObserver(&view_state_observer);

  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  // Changing the side panel entry after it is opened, should not notify
  // observers.
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->RemoveSidePanelViewStateObserver(&view_state_observer);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, NotifyingSidePanelObservers) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  MockSidePanelViewStateObserver view_state_observer;
  EXPECT_CALL(view_state_observer, OnSidePanelDidOpen()).Times(3);
  EXPECT_CALL(view_state_observer, OnSidePanelDidClose()).Times(2);

  coordinator()->AddSidePanelViewStateObserver(&view_state_observer);

  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->RemoveSidePanelViewStateObserver(&view_state_observer);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       RemovingObserverDoesNotIncrementCount) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  MockSidePanelViewStateObserver view_state_observer;
  EXPECT_CALL(view_state_observer, OnSidePanelDidClose()).Times(1);
  coordinator()->AddSidePanelViewStateObserver(&view_state_observer);
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  coordinator()->RemoveSidePanelViewStateObserver(&view_state_observer);

  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SidePanelToggleWithEntriesTest) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Show reading list sidepanel.
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  // Toggle reading list sidepanel to close.
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  // Toggling reading list followed by bookmarks shows the reading list side
  // panel followed by the bookmarks side panel.
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ShowOpensSidePanel) {
  Init();
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  // Verify that bookmarks is selected.
  EXPECT_EQ(GetTitleText(),
            l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwapBetweenTabsWithBookmarksOpen) {
  Init();
  // Verify side panel opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);

  // Verify switching tabs does not change side panel visibility or entry seen
  // if it is in the global registry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwapBetweenTabsWithReadingListOpen) {
  Init();
  // Open side panel and switch to kReadingList and verify the active entry is
  // updated.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  coordinator()->Show(SidePanelEntry::Id::kReadingList);

  // Verify switching tabs does not change entry seen if it is in the global
  // registry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kReadingList);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ContextualEntryDeregistered) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);

  // Verify the first tab has kShoppingInsights.
  tabs::TabModel* tab =
      browser()->GetBrowserView().browser()->tab_strip_model()->GetTabAtIndex(
          0);
  SidePanelRegistry* registry = tab->tab_features()->side_panel_registry();
  SidePanelEntryKey key(SidePanelEntry::Id::kShoppingInsights);

  EXPECT_TRUE(registry->GetEntryForKey(key));
  registry->Deregister(key);
  EXPECT_FALSE(registry->GetEntryForKey(key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ContextualEntryDeregisteredWhileVisible) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);

  // Verify the first tab's registry does not have an active entry.
  tabs::TabModel* tab =
      browser()->GetBrowserView().browser()->tab_strip_model()->GetTabAtIndex(
          0);
  SidePanelRegistry* tab_registry = tab->tab_features()->side_panel_registry();
  SidePanelEntryKey key(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(tab_registry->active_entry().has_value());

  // Show an entry from the first tab's registry
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(tab_registry->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);

  // Deregister kShoppingInsights from the first tab.
  global_registry()->Deregister(key);
  tab_registry->Deregister(key);

  // Verify the panel defaults back to the last visible global entry or the
  // reading list.
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(tab_registry->active_entry().has_value());
}

// Test that the side panel closes if a contextual entry is deregistered while
// visible when no global entries have been shown since the panel was opened.
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    ContextualEntryDeregisteredWhileVisibleClosesPanelIfNoLastSeenGlobalEntryExists) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());

  // Verify the first tab's registry has an active entry.
  tabs::TabModel* tab =
      browser()->GetBrowserView().browser()->tab_strip_model()->GetTabAtIndex(
          0);
  SidePanelRegistry* tab_registry = tab->tab_features()->side_panel_registry();
  SidePanelEntryKey key(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(tab_registry->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);

  // Deregister kShoppingInsights from the first tab.
  tab_registry->Deregister(key);
  EXPECT_FALSE(tab_registry->GetEntryForKey(key));

  // Verify the panel closes.
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  EXPECT_FALSE(tab_registry->active_entry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ShowContextualEntry) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwapBetweenTwoContextualEntryWithTheSameId) {
  Init();
  // Open shopping insights for the first tab.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  auto* reading_list_entry =
      coordinator()->GetCurrentSidePanelEntryForTesting();
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  auto* shopping_entry1 = coordinator()->GetCurrentSidePanelEntryForTesting();

  // Switch to the second tab and open shopping insights.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_EQ(reading_list_entry,
            coordinator()->GetCurrentSidePanelEntryForTesting());
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_NE(shopping_entry1,
            coordinator()->GetCurrentSidePanelEntryForTesting());

  // Switch back to the first tab.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_EQ(shopping_entry1,
            coordinator()->GetCurrentSidePanelEntryForTesting());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwapBetweenTabsAfterNavigatingToContextualEntry) {
  Init();
  // Open side panel and verify it opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  auto* bookmarks_entry = coordinator()->GetCurrentSidePanelEntryForTesting();

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  auto* shopping_entry = coordinator()->GetCurrentSidePanelEntryForTesting();

  // Switch to a tab where this contextual entry is not available and verify we
  // fall back to the last seen global entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(bookmarks_entry,
            coordinator()->GetCurrentSidePanelEntryForTesting());

  // Switch back to the tab where the contextual entry was visible and verify it
  // is shown.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(shopping_entry,
            coordinator()->GetCurrentSidePanelEntryForTesting());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       TogglePanelWithContextualEntryShowing) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel and verify it opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(), SidePanelEntry::Id::kBookmarks);

  // Switch to a different global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kReadingList);

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kShoppingInsights);

  // Close the side panel and verify the contextual registry's last active entry
  // remains set.
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       TogglePanelWithGlobalEntryShowingWithTabSwitch) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel and verify it opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kShoppingInsights);

  // Switch to a global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kReadingList);

  // Close the side panel and verify the global registry's last active entry
  // is set and the contextual registry's last active entry is reset.
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->last_active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and open a contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->last_active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kShoppingInsights);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       TogglePanelWithContextualEntryShowingWithTabSwitch) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel and verify it opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kShoppingInsights);

  // Close the side panel and verify the contextual registry's last active entry
  // is set.
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab, open the side panel, and verify the contextual
  // registry's last active entry is still set.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Close the side panel and verify the contextual registry's last active entry
  // is still set.
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    SwitchBetweenTabWithGlobalEntryAndTabWithLastActiveContextualEntry) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel to kBookmarks.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and open a contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);

  // Close the side panel and verify the contextual registry's last active entry
  // is set.
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->last_active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);

  // Switch back to the first tab and open the side panel.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->last_active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);

  // Switch back to the second tab and verify that the last active global entry
  // is set.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->last_active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);

  // Close the side panel and verify that the last active contextual entry is
  // reset.
  coordinator()->Close();
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->last_active_entry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwitchBetweenTabWithContextualEntryAndTabWithNoEntry) {
  Init();
  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    SwitchBetweenTabWithContextualEntryAndTabWithNoEntryWhenThereIsALastActiveGlobalEntry) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->last_active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwitchBackToTabWithPreviouslyVisibleContextualEntry) {
  Init();
  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a global entry and verify the contextual entry is no longer
  // active.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different tab and verify state.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the original tab and verify the contextual entry is not
  // active or showing.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwitchBackToTabWithContextualEntryAfterClosingGlobal) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Open a global entry and verify.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Verify the panel closes but the first tab still has an active entry.
  coordinator()->Close();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Verify returning to the first tab reopens the side panel to the active
  // contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

// Verify that side panels maintain individual widths when the
// #side-panel-resizing flag is enabled. In this case, the bookmarks and reading
// list side panels should be able to have independent widths.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, SidePanelWidthPreference) {
  Init();
  ASSERT_TRUE(&browser()->GetBrowserView());
  SidePanel* side_panel = browser()->GetBrowserView().unified_side_panel();
  ASSERT_TRUE(side_panel);

  PrefService* prefs =
      browser()->GetBrowserView().browser()->profile()->GetPrefs();
  auto& dict = prefs->GetDict(prefs::kSidePanelIdToWidth);
  const std::string bookmarks_side_panel_id =
      SidePanelEntryIdToString(SidePanelEntry::Id::kBookmarks);
  const std::string reading_list_side_panel_id =
      SidePanelEntryIdToString(SidePanelEntry::Id::kReadingList);

  // Verify both side panels do not have a stored width value.
  EXPECT_FALSE(dict.FindInt(bookmarks_side_panel_id).has_value());
  EXPECT_FALSE(dict.FindInt(reading_list_side_panel_id).has_value());

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  const int initial_side_panel_width = side_panel->width();
  const int expected_bookmark_width = initial_side_panel_width + 100;

  // Resize the bookmarks side panel.
  side_panel->OnResize(-100, false);

  // Ensure the bookmark width is updated accordingly after resizing.
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(side_panel->width(), expected_bookmark_width);

  // Verify the preference value is updated after resize.
  EXPECT_EQ(expected_bookmark_width, prefs->GetDict(prefs::kSidePanelIdToWidth)
                                         .FindInt(bookmarks_side_panel_id));

  // Show the reading list side panel.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());

  // Verify the reading list side panel keeps the resized width.
  EXPECT_EQ(initial_side_panel_width, side_panel->width());
  const int expected_reading_list_width = initial_side_panel_width + 50;

  // Resize the reading list side panel.
  side_panel->OnResize(-50, false);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());

  // Ensure the reading list width is updated accordingly after resizing.
  EXPECT_EQ(side_panel->width(), expected_reading_list_width);

  // Verify the preference value is updated after resize.
  EXPECT_EQ(expected_reading_list_width,
            prefs->GetDict(prefs::kSidePanelIdToWidth)
                .FindInt(reading_list_side_panel_id));

  // Show the bookmarks side panel again to verify we use the correct width.
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());

  EXPECT_NE(expected_reading_list_width, side_panel->width());
  EXPECT_EQ(expected_bookmark_width, side_panel->width());
}

class TestSidePanelObserver : public SidePanelEntryObserver {
 public:
  explicit TestSidePanelObserver(SidePanelRegistry* registry)
      : registry_(registry) {}
  ~TestSidePanelObserver() override = default;

  void OnEntryHidden(SidePanelEntry* entry) override {
    registry_->Deregister(entry->key());
  }

  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override {
    last_entry_will_hide_entry_id_ = entry->key().id();
    last_entry_will_hide_reason_ = reason;
  }

  std::optional<SidePanelEntry::Id> last_entry_will_hide_entry_id_;
  std::optional<SidePanelEntryHideReason> last_entry_will_hide_reason_;

 private:
  raw_ptr<SidePanelRegistry> registry_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       EntryDeregistersOnBeingHiddenFromSwitchToOtherEntry) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);

  // Create an observer that deregisters the entry once it is hidden.
  auto observer =
      std::make_unique<TestSidePanelObserver>(contextual_registries_[0]);
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kAboutThisSite,
      base::BindRepeating([]() { return std::make_unique<views::View>(); }));
  entry->AddObserver(observer.get());
  contextual_registries_[0]->Register(std::move(entry));
  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);

  // Switch to another entry.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);

  // Verify that the previous entry has deregistered and is hidden.
  EXPECT_THAT(observer->last_entry_will_hide_entry_id_,
              testing::Optional(SidePanelEntry::Id::kAboutThisSite));
  EXPECT_THAT(observer->last_entry_will_hide_reason_,
              testing::Optional(SidePanelEntryHideReason::kReplaced));
  EXPECT_FALSE(contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite)));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       EntryDeregistersOnBeingHiddenFromSidePanelClose) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);

  // Create an observer that deregisters the entry once it is hidden.
  auto observer =
      std::make_unique<TestSidePanelObserver>(contextual_registries_[0]);
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kAboutThisSite,
      base::BindRepeating([]() { return std::make_unique<views::View>(); }));
  entry->AddObserver(observer.get());
  contextual_registries_[0]->Register(std::move(entry));
  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);

  // Close the side panel.
  coordinator()->Close();

  // Verify that the previous entry has deregistered and is hidden.
  EXPECT_THAT(observer->last_entry_will_hide_entry_id_,
              testing::Optional(SidePanelEntry::Id::kAboutThisSite));
  EXPECT_THAT(observer->last_entry_will_hide_reason_,
              testing::Optional(SidePanelEntryHideReason::kSidePanelClosed));
  EXPECT_FALSE(contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite)));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ShouldNotRecreateTheSameEntry) {
  Init();
  // Switch to a tab without a contextual entry for lens, so that Show() shows
  // the global entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);

  int count = 0;
  global_registry()->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kLens, base::BindRepeating(
                                     [](int* count) {
                                       (*count)++;
                                       return std::make_unique<views::View>();
                                     },
                                     &count)));
  coordinator()->Show(SidePanelEntry::Id::kLens);
  ASSERT_EQ(1, count);
  coordinator()->Show(SidePanelEntry::Id::kLens);
  ASSERT_EQ(1, count);
}

// Side panel closes if the active entry is de-registered when open.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       GlobalEntryDeregisteredWhenVisible) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));

  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
}

// Test that a crash does not occur when the browser is closed when the side
// panel view is shown but before the entry to be displayed has finished
// loading. Regression for crbug.com/1408947.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       BrowserClosedBeforeEntryLoaded) {
  Init();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  // Allow content delays to more closely mimic real behavior.
  coordinator()->SetNoDelaysForTesting(false);
  coordinator()->Close();
  browser()->GetBrowserView().Close();
}

// Test that Show() shows the contextual extension entry if available for the
// current tab. Otherwise it shows the global extension entry. Note: only
// extensions will be able to have their entries exist in both the global and
// contextual registries.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ShowGlobalAndContextualExtensionEntries) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  // Add extension
  scoped_refptr<const extensions::Extension> extension =
      LoadSidePanelExtension("extension");
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    extension->id());
  global_registry()->Register(CreateEntry(extension_key));
  contextual_registries_[0]->Register(CreateEntry(extension_key));

  coordinator()->Show(extension_key);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());

  // Switch to a tab that does not have an extension entry registered for its
  // contextual registry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(extension_key);
  EXPECT_EQ(global_registry()->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());
}

// Test that a new contextual extension entry gets shown if it's registered for
// the active tab and the global extension entry is showing.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, RegisterExtensionEntries) {
  Init();

  // Add extension
  scoped_refptr<const extensions::Extension> extension =
      LoadSidePanelExtension("extension");

  // Give extension access to global side panel.
  RunSetOptions(*extension, /*tab_id=*/std::nullopt,
                /*path=*/"panel.html",
                /*enabled=*/true);

  // Show the global side panel.
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    extension->id());
  coordinator()->Show(extension_key);
  EXPECT_FALSE(coordinator()->current_key()->tab_handle.has_value());

  // Give extension access to tab side panel.
  // Tab entry should immediately be shown.
  RunSetOptions(*extension,
                /*tab_id=*/
                sessions::SessionTabHelper::IdForTab(
                    browser()->GetActiveTabInterface()->GetContents())
                    .id(),
                /*path=*/"panel.html",
                /*enabled=*/true);
  EXPECT_TRUE(coordinator()->current_key()->tab_handle.has_value());
}

// Test that if global or contextual entries are deregistered, and if it exists,
// the global extension entry is shown if the active tab's extension entry is
// deregistered.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, DeregisterExtensionEntries) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Make sure the second tab is active.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);

  // Add extension.
  scoped_refptr<const extensions::Extension> extension =
      LoadSidePanelExtension("extension");
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    extension->id());

  // Registers an entry in the global and active contextual registry.
  auto register_entries = [this, &extension_key]() {
    contextual_registries_[1]->Register(CreateEntry(extension_key));
    global_registry()->Register(CreateEntry(extension_key));
  };

  register_entries();

  // The contextual entry should be shown.
  coordinator()->Show(extension_key);
  EXPECT_EQ(contextual_registries_[1]->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());

  // If the contextual entry is deregistered while there exists a global entry,
  // the global entry should be shown.
  contextual_registries_[1]->Deregister(extension_key);
  EXPECT_EQ(global_registry()->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());

  // The side panel should be closed after the global entry is deregistered.
  global_registry()->Deregister(extension_key);
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  register_entries();
  coordinator()->Show(extension_key);

  // The contextual entry should still be shown after the global entry is
  // deregistered.
  global_registry()->Deregister(extension_key);
  EXPECT_EQ(contextual_registries_[1]->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());

  contextual_registries_[1]->Deregister(extension_key);
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
}

// Test that an extension with only contextual entries should behave like other
// contextual entry types when switching tabs.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ExtensionEntriesTabSwitchNoGlobalEntry) {
  Init();
  // Switch to the first tab, then register and show an extension entry on its
  // contextual registry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  // Add extension.
  scoped_refptr<const extensions::Extension> extension =
      LoadSidePanelExtension("extension");
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    extension->id());
  contextual_registries_[0]->Register(CreateEntry(extension_key));
  coordinator()->Show(extension_key);

  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());

  // Switch to the second tab. Since there is no active contextual/global entry
  // and no global entry with `extension_key`, the side panel should close.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(coordinator()->IsSidePanelShowing());
}

// Test that an extension with both contextual and global entries should behave
// like global entries when switching tabs and its entries take precedence over
// all other entries except active contextual entries (this case is covered in
// ExtensionEntriesTabSwitchWithActiveContextualEntry).
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ExtensionEntriesTabSwitchGlobalEntry) {
  Init();
  // Add extension.
  scoped_refptr<const extensions::Extension> extension =
      LoadSidePanelExtension("extension");
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    extension->id());
  contextual_registries_[0]->Register(CreateEntry(extension_key));
  global_registry()->Register(CreateEntry(extension_key));

  // Switching from a tab showing the extension's active entry to a
  // tab with no active contextual entry should show the extension's entry
  // (global in this case).
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(extension_key);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());

  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(global_registry()->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               extension_key);

  // Reset the active entry on the first tab.
  contextual_registries_[0]->ResetActiveEntry();

  // Switching from a tab with the global extension entry to a tab with a
  // contextual extension entry should show the contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               extension_key);

  // Now register a reading list entry in the global registry and show it on the
  // second tab.
  SidePanelEntry::Key reading_list_key(SidePanelEntry::Id::kReadingList);
  global_registry()->Register(CreateEntry(reading_list_key));
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(reading_list_key);

  // Show the extension's contextual entry on the first tab.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(extension_key);

  // Switch to the second tab. The extension's global entry should show and be
  // the active entry in the global registry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(global_registry()->GetEntryForKey(extension_key),
            coordinator()->GetCurrentSidePanelEntryForTesting());
  VerifyEntryExistenceAndValue(global_registry()->active_entry(),
                               extension_key);

  // Show shopping insights on the second tab.
  coordinator()->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights));
  // Reset the active entry on the first tab.
  contextual_registries_[0]->ResetActiveEntry();
}

// Test that when switching tabs while an extension's entry is showing, the new
// tab's active contextual entry should still take precedence over the
// extensions' entries.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ExtensionEntriesTabSwitchWithActiveContextualEntry) {
  Init();
  SidePanelEntry::Key shopping_key(SidePanelEntry::Id::kShoppingInsights);
  // Add extension.
  scoped_refptr<const extensions::Extension> extension =
      LoadSidePanelExtension("extension");
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    extension->id());
  contextual_registries_[0]->Register(CreateEntry(extension_key));
  global_registry()->Register(CreateEntry(extension_key));

  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(shopping_key);

  // Show the extension's global entry on the second tab.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(extension_key);
}

// Tests that DeregisterAndReturnView returns the deregistered entry's view if
// it exists, whether or not the entry is showing.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, DeregisterAndReturnView) {
  Init();
  // A view with a counter as an internal state, used to check that the correct
  // view is returned by DeregisterAndReturnView.
  class ViewWithCounter : public views::View {
   public:
    explicit ViewWithCounter(int counter) : views::View(), counter_(counter) {}
    ~ViewWithCounter() override = default;

    int counter() { return counter_; }

   private:
    int counter_ = 0;
  };

  SidePanelEntry::Key shopping_key(SidePanelEntry::Id::kShoppingInsights);
  // Add extension.
  scoped_refptr<const extensions::Extension> extension =
      LoadSidePanelExtension("extension");
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    extension->id());

  auto create_entry_with_counter = [](const SidePanelEntry::Key& key,
                                      int counter) {
    return std::make_unique<SidePanelEntry>(
        key, base::BindRepeating(
                 [](int counter) -> std::unique_ptr<views::View> {
                   return std::make_unique<ViewWithCounter>(counter);
                 },
                 counter));
  };

  // Register the entry but don't show it.
  global_registry()->Register(create_entry_with_counter(extension_key, 11));

  // Since the entry was never shown, its view was never created and
  // `returned_view` should be null.
  std::unique_ptr<views::View> returned_view =
      SidePanelUtil::DeregisterAndReturnView(global_registry(), extension_key);
  EXPECT_FALSE(returned_view);

  // Register the entry and show it.
  global_registry()->Register(create_entry_with_counter(extension_key, 22));
  coordinator()->Show(extension_key);

  // Since the entry was shown, its view was created. Check that the correct
  // view is returned by checking its state that was set at creation time.
  returned_view =
      SidePanelUtil::DeregisterAndReturnView(global_registry(), extension_key);
  ASSERT_TRUE(returned_view);
  EXPECT_EQ(22, static_cast<ViewWithCounter*>(returned_view.get())->counter());

  // Register the entry, show it, then show another entry so the entry for
  // `extension_key` has its view cached.
  global_registry()->Register(create_entry_with_counter(extension_key, 33));
  coordinator()->Show(extension_key);
  coordinator()->Show(shopping_key);

  // Since the entry was shown, its view was created. Check that the correct
  // view is returned by checking its state that was set at creation time.
  returned_view =
      SidePanelUtil::DeregisterAndReturnView(global_registry(), extension_key);
  ASSERT_TRUE(returned_view);
  EXPECT_EQ(33, static_cast<ViewWithCounter*>(returned_view.get())->counter());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, SidePanelTitleUpdates) {
  Init();
  SetUpPinningTest();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_EQ(GetTitleText(),
            l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE));

  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_EQ(GetTitleText(), l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));

  // Checks that the title updates even for contextual side panels
  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);
  EXPECT_EQ(GetTitleText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SidePanelPinButtonsHideInGuestMode) {
  // Check that pin button shows in normal window.
  coordinator()->SetNoDelaysForTesting(true);
  coordinator()->DisableAnimationsForTesting();
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(coordinator()->GetHeaderPinButtonForTesting()->GetVisible());

  // Make a guest window. This process can be either synchronous or
  // asynchronous, so use RunUntil.
  Browser* guest_browser = nullptr;
  profiles::SwitchToGuestProfile(base::BindOnce(
      [](Browser** output, Browser* browser) { *output = browser; },
      &guest_browser));
  ASSERT_TRUE(base::test::RunUntil([&]() { return guest_browser != nullptr; }));
  ASSERT_TRUE(guest_browser);
  ASSERT_TRUE(guest_browser->profile()->IsGuestSession());

  // Check that pin button does not show in guest window.
  auto* coordinator = guest_browser->GetFeatures().side_panel_coordinator();
  coordinator->SetNoDelaysForTesting(true);
  coordinator->DisableAnimationsForTesting();
  coordinator->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(coordinator->GetHeaderPinButtonForTesting()->GetVisible());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies that clicking the pin button on an extensions side panel, pins the
// extension in ToolbarActionModel.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ExtensionSidePanelHasPinButton) {
  Init();
  SetUpPinningTest();
  EXPECT_FALSE(coordinator()->IsSidePanelShowing());

  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithSidePanel("extension", std::nullopt);

  coordinator()->Show(GetKeyForExtension(extension->id()));
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      GetKeyForExtension(extension->id())));

  views::ToggleImageButton* pin_button =
      coordinator()->GetHeaderPinButtonForTesting();
  EXPECT_TRUE(pin_button->GetVisible());
  EXPECT_FALSE(pin_button->GetToggled());

  ToolbarActionsModel* model = ToolbarActionsModel::Get(browser()->profile());
  EXPECT_TRUE(model->pinned_action_ids().empty());

  WaitForExtensionsContainerAnimation();
  ClickButton(pin_button);
  WaitForExtensionsContainerAnimation();

  EXPECT_TRUE(pin_button->GetVisible());
  EXPECT_TRUE(pin_button->GetToggled());
  EXPECT_EQ(1u, model->pinned_action_ids().size());
}

class SidePanelCoordinatorLensOverlayTest : public SidePanelCoordinatorTest {
 public:
  SidePanelCoordinatorLensOverlayTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kSidePanelResizing, lens::features::kLensOverlay}, {});
  }
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLensOverlayTest,
                       ShowMoreInfoButtonWhenCallbackProvided) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kLensOverlayResults);
  VerifyEntryExistenceAndValue(contextual_registries_[1]->active_entry(),
                               SidePanelEntry::Id::kLensOverlayResults);
  views::ImageButton* more_info_button =
      coordinator()->GetHeaderMoreInfoButtonForTesting();
  EXPECT_TRUE(more_info_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLensOverlayTest,
                       HideMoreInfoButtonWhenNoCallbackProvided) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kLens);
  VerifyEntryExistenceAndValue(contextual_registries_[1]->active_entry(),
                               SidePanelEntry::Id::kLens);
  views::ImageButton* more_info_button =
      coordinator()->GetHeaderMoreInfoButtonForTesting();
  EXPECT_FALSE(more_info_button->GetVisible());
}

// Test that the SidePanelCoordinator behaves and updates corrected when dealing
// with entries that load/display asynchronously.
class SidePanelCoordinatorLoadingContentTest : public SidePanelCoordinatorTest {
 public:
  void Init() override {
    // Intentionally do not call SidePanelCoordinatorTest::Init().

    AddTabToBrowser(GURL("http://foo1.com"));
    AddTabToBrowser(GURL("http://foo2.com"));

    // Add a kShoppingInsights entry to the global registry with loading content
    // not available.
    std::unique_ptr<SidePanelEntry> entry1 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kShoppingInsights, base::BindRepeating([]() {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(false);
          return view;
        }));
    loading_content_entry1_ = entry1.get();
    EXPECT_TRUE(global_registry()->Register(std::move(entry1)));

    // Add a kLens entry to the global registry with loading content not
    // available.
    std::unique_ptr<SidePanelEntry> entry2 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLens, base::BindRepeating([]() {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(false);
          return view;
        }));
    loading_content_entry2_ = entry2.get();
    EXPECT_TRUE(global_registry()->Register(std::move(entry2)));

    // Add a kAboutThisSite entry to the global registry with content available.
    std::unique_ptr<SidePanelEntry> entry3 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kAboutThisSite, base::BindRepeating([]() {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(true);
          return view;
        }));
    loaded_content_entry1_ = entry3.get();
    EXPECT_TRUE(global_registry()->Register(std::move(entry3)));
  }

  raw_ptr<SidePanelEntry, DanglingUntriaged> loading_content_entry1_;
  raw_ptr<SidePanelEntry, DanglingUntriaged> loading_content_entry2_;
  raw_ptr<SidePanelEntry, DanglingUntriaged> loaded_content_entry1_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLoadingContentTest,
                       ContentDelaysForLoadingContent) {
  Init();
  coordinator()->Show(loading_content_entry1_->key().id());
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  // A loading entry's view should be stored as the cached view and be
  // unavailable.
  views::View* loading_content = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy->IsAvailable());
  // Set the content proxy to available.
  loading_content_proxy->SetAvailable(true);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  // Switch to another entry that has loading content.
  coordinator()->Show(loading_content_entry2_->key().id());
  loading_content = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  loading_content_proxy =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy->IsAvailable());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loading_content_entry1_->key().id());
  // Set as available and make sure the title has updated.
  loading_content_proxy->SetAvailable(true);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loading_content_entry2_->key().id());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLoadingContentTest,
                       TriggerSwitchToNewEntryDuringContentLoad) {
  Init();
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  coordinator()->Show(loaded_content_entry1_->key().id());
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loaded_content_entry1_->key().id());

  // Switch to loading_content_entry1_ that has loading content.
  coordinator()->Show(loading_content_entry1_->key().id());
  views::View* loading_content1 = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content1, nullptr);
  SidePanelContentProxy* loading_content_proxy1 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content1);
  EXPECT_FALSE(loading_content_proxy1->IsAvailable());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loaded_content_entry1_->key().id());
  // Verify the loading_content_entry1_ is the loading entry.
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(),
            loading_content_entry1_);

  // While that entry is loading, switch to a different entry with content that
  // needs to load.
  coordinator()->Show(loading_content_entry2_->key().id());
  views::View* loading_content2 = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content2, nullptr);
  SidePanelContentProxy* loading_content_proxy2 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content2);
  EXPECT_FALSE(loading_content_proxy2->IsAvailable());
  // Verify the loading_content_entry2_ is no longer the loading entry.
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(),
            loading_content_entry2_);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loaded_content_entry1_->key().id());

  // Set loading_content_entry1_ as available and verify it is not made the
  // active entry.
  loading_content_proxy1->SetAvailable(true);
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(),
            loading_content_entry2_);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loaded_content_entry1_->key().id());

  // Set loading_content_entry2_ as available and verify it is made the active
  // entry.
  loading_content_proxy2->SetAvailable(true);
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(), nullptr);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loading_content_entry2_->key().id());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLoadingContentTest,
                       TriggerSwitchToCurrentVisibleEntryDuringContentLoad) {
  Init();
  coordinator()->Show(loading_content_entry1_->key().id());
  EXPECT_FALSE(browser()->GetBrowserView().unified_side_panel()->GetVisible());
  // A loading entry's view should be stored as the cached view and be
  // unavailable.
  views::View* loading_content = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy1 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy1->IsAvailable());
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(),
            loading_content_entry1_);
  // Set the content proxy to available.
  loading_content_proxy1->SetAvailable(true);
  EXPECT_TRUE(browser()->GetBrowserView().unified_side_panel()->GetVisible());

  // Switch to loading_content_entry2_ that has loading content.
  coordinator()->Show(loading_content_entry2_->key().id());
  loading_content = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy2 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy2->IsAvailable());
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loading_content_entry1_->key().id());
  // Verify the loading_content_entry2_ is the loading entry.
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(),
            loading_content_entry2_);

  // While that entry is loading, switch back to the currently showing entry.
  coordinator()->Show(loading_content_entry1_->key().id());
  // Verify the loading_content_entry2_ is no longer the loading entry.
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(), nullptr);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loading_content_entry1_->key().id());

  // Set loading_content_entry2_ as available and verify it is not made the
  // active entry.
  loading_content_proxy2->SetAvailable(true);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loading_content_entry1_->key().id());

  // Show loading_content_entry2_ and verify it shows without availability
  // needing to be set again.
  coordinator()->Show(loading_content_entry2_->key().id());
  EXPECT_EQ(coordinator()->GetLoadingEntryForTesting(), nullptr);
  EXPECT_EQ(coordinator()->GetCurrentEntryId(),
            loading_content_entry2_->key().id());
}
