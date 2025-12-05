// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_api.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/lens/lens_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"

using testing::_;

namespace {

// Creates a basic SidePanelEntry for the given `key` that returns an empty view
// when shown.
std::unique_ptr<SidePanelEntry> CreateEntry(const SidePanelEntry::Key& key) {
  return std::make_unique<SidePanelEntry>(
      key, base::BindRepeating([](SidePanelEntryScope&) {
        return std::make_unique<views::View>();
      }),
      /*default_content_width_callback=*/base::NullCallback());
}

}  // namespace

class SidePanelCoordinatorTest : public InProcessBrowserTest {
 public:
  SidePanelCoordinatorTest() = default;
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
        SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights),
        base::BindRepeating([](SidePanelEntryScope&) {
          return std::make_unique<views::View>();
        }),
        /*default_content_width_callback=*/base::NullCallback()));
    contextual_registries_.push_back(registry);

    // Add some entries to the second tab.
    browser()->tab_strip_model()->ActivateTabAt(1);
    registry = browser()
                   ->GetActiveTabInterface()
                   ->GetTabFeatures()
                   ->side_panel_registry();
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(SidePanelEntry::Id::kLens),
        base::BindRepeating([](SidePanelEntryScope&) {
          return std::make_unique<views::View>();
        }),
        /*default_content_width_callback=*/base::NullCallback()));
    contextual_registries_.push_back(browser()
                                         ->GetActiveTabInterface()
                                         ->GetTabFeatures()
                                         ->side_panel_registry());

    // Add a kLensOverlayResults entry to the contextual registry for the second
    // tab.
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults),
        base::BindRepeating([](SidePanelEntryScope&) {
          return std::make_unique<views::View>();
        }),
        /*open_in_new_tab_url_callback=*/base::NullCallback(),
        base::BindRepeating([]() {
          return std::unique_ptr<ui::MenuModel>(
              new ui::SimpleMenuModel(nullptr));
        }),
        /*default_content_width_callback=*/base::NullCallback()));
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights),
        base::BindRepeating([](SidePanelEntryScope&) {
          return std::make_unique<views::View>();
        }),
        /*default_content_width_callback=*/base::NullCallback()));

    coordinator()->SetNoDelaysForTesting(true);
  }

  void SetUpPinningTest() {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    auto* const registry = SidePanelRegistry::GetDeprecated(web_contents);
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
        base::BindRepeating([](SidePanelEntryScope&) {
          return std::make_unique<views::View>();
        }),
        /*default_content_width_callback=*/base::NullCallback()));
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

  SidePanelHeader* GetHeader() {
    return browser()
        ->GetBrowserView()
        .contents_height_side_panel()
        ->GetHeaderView<SidePanelHeader>();
  }

  std::u16string_view GetTitleText() {
    return GetHeader()->panel_title()->GetText();
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

  SidePanelRegistry* GetActiveTabRegistry() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->side_panel_registry();
  }

  // TODO(https://crbug.com/454583671): Eliminate this in favor of something
  // that actually returns the specific width needed by the test, or else find
  // some other way to calculate this in the test itself.
  int GetMinWebContentsWidth() const {
    return static_cast<BrowserViewLayout*>(
               browser()->GetBrowserView().GetLayoutManager())
        ->GetMinWebContentsWidthForTesting();
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
                           path_arg.c_str(), base::ToString(enabled));
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

    extensions::PermissionsUpdater(browser()->profile())
        .GrantActivePermissions(extension.get());
    extensions::ExtensionRegistrar::Get(browser()->profile())
        ->AddExtension(extension);

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

  SidePanelCoordinator* coordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }

  SidePanelRegistry* global_registry() {
    return SidePanelRegistry::From(browser());
  }

  std::vector<raw_ptr<SidePanelRegistry, DanglingUntriaged>>
      contextual_registries_;
};

class SidePanelCoordinatorWithSideBySideTest : public SidePanelCoordinatorTest {
 public:
  SidePanelCoordinatorWithSideBySideTest() {
    scoped_feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ToggleSidePanel) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, VerifyFocusOrder) {
  Init();
  coordinator()->DisableAnimationsForTesting();
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  SidePanel* side_panel =
      browser()->GetBrowserView().contents_height_side_panel();
  auto children_in_focus_order = side_panel->GetChildrenFocusList();
  auto resize_it =
      std::find(children_in_focus_order.begin(), children_in_focus_order.end(),
                side_panel->resize_area_for_testing());
  EXPECT_NE(resize_it, children_in_focus_order.end());
  auto header_it = std::find(children_in_focus_order.begin(),
                             children_in_focus_order.end(), GetHeader());
  EXPECT_NE(header_it, children_in_focus_order.end());
  auto content_it =
      std::find(children_in_focus_order.begin(), children_in_focus_order.end(),
                side_panel->GetContentParentView());
  EXPECT_NE(content_it, children_in_focus_order.end());
  // Verify the order is resize area -> header -> content.
  EXPECT_LT(resize_it, header_it);
  EXPECT_LT(header_it, content_it);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, OpenWhileClosing) {
  Init();

  class SidePanelEntryObserverFuture : public SidePanelEntryObserver {
   public:
    explicit SidePanelEntryObserverFuture(SidePanelEntry* entry)
        : entry_(entry) {
      entry_->AddObserver(this);
    }

    ~SidePanelEntryObserverFuture() override { entry_->RemoveObserver(this); }

    void OnEntryShown(SidePanelEntry* entry) override {
      shown_future_.SetValue();
    }
    void OnEntryHideCancelled(SidePanelEntry* entry) override {
      hide_cancelled_future_.SetValue();
    }

    bool WaitForShown() { return shown_future_.WaitAndClear(); }
    bool WaitForHideCancelled() {
      return hide_cancelled_future_.WaitAndClear();
    }

   private:
    raw_ptr<SidePanelEntry> entry_;
    base::test::TestFuture<void> shown_future_;
    base::test::TestFuture<void> hide_cancelled_future_;
  };

  auto* entry = global_registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  ASSERT_TRUE(entry);

  SidePanelEntryObserverFuture observer(entry);

  // Wait for the side panel to be visible and fully shown.
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  EXPECT_TRUE(observer.WaitForShown());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().contents_height_side_panel()->state() ==
           SidePanel::State::kOpen;
  }));

  // Closing the side panel is asynchronous.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->state(),
            SidePanel::State::kClosing);

  // Opening the same entry should cancel the close.
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  EXPECT_TRUE(observer.WaitForHideCancelled());
  auto state =
      browser()->GetBrowserView().contents_height_side_panel()->state();
  EXPECT_TRUE(state == SidePanel::State::kOpen ||
              state == SidePanel::State::kOpening);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, OpenAndCloseWithoutAnimation) {
  coordinator()->DisableAnimationsForTesting();
  Init();
  // Since there is no animation opening/closing the side-panel should be
  // synchronous.
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->state(),
            SidePanel::State::kOpen);

  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->state(),
            SidePanel::State::kClosed);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidth) {
  Init();
  // Set side panel to left-aligned so positive resize increments mean an
  // increase in side panel width.
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  coordinator()->DisableAnimationsForTesting();

  const int min_side_panel_width = browser()
                                       ->GetBrowserView()
                                       .contents_height_side_panel()
                                       ->GetMinimumSize()
                                       .width();

  // Set the browser width so that two thirds of the browser would be larger
  // than the minimum side panel width.
  gfx::Rect original_browser_bounds(browser()->GetBrowserView().GetBounds());
  gfx::Rect new_bounds(original_browser_bounds);
  new_bounds.set_width(min_side_panel_width * 3);
  // Explicitly restore the browser window on ChromeOS, as it would otherwise
  // be maximized and the SetBounds call would be a no-op.
#if BUILDFLAG(IS_CHROMEOS)
  browser()->GetBrowserView().Restore();
#endif
  browser()->GetBrowserView().SetBounds(new_bounds);

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  int browser_width = browser()->GetBrowserView().GetLocalBounds().width();
  int two_thirds_browser_width = browser_width * 2 / 3;
  // Select a starting width less than the min width.
  const int starting_width = min_side_panel_width - 1;
  browser()->GetBrowserView().contents_height_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  // Verify the side panel will is at the min width.
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            min_side_panel_width);

  // Increment the side panel width so that it is larger than the min width but
  // less than two thirds of the browser width.
  int increment = (two_thirds_browser_width - min_side_panel_width) / 2;
  browser()->GetBrowserView().contents_height_side_panel()->OnResize(increment,
                                                                     true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  // Verify the side panel is at its preferred width.
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->GetPreferredSize()
                .width());

  // Increment the side panel width so that it is larger than two thirds of the
  // browser width.
  increment = (two_thirds_browser_width + 1) -
              browser()->GetBrowserView().contents_height_side_panel()->width();
  browser()->GetBrowserView().contents_height_side_panel()->OnResize(increment,
                                                                     true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());

  // Verify the side panel width is capped at two thirds of the browser width.
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            two_thirds_browser_width);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ReadAnythingSidePanelWidthNotCappedAtTwoThirds) {
  Init();
  // Set side panel to left-aligned so positive resize increments mean an
  // increase in side panel width.
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  coordinator()->DisableAnimationsForTesting();

  const int min_side_panel_width = browser()
                                       ->GetBrowserView()
                                       .contents_height_side_panel()
                                       ->GetMinimumSize()
                                       .width();

  // Set the browser width so that two thirds of the browser would be larger
  // than the minimum side panel width.
  gfx::Rect original_browser_bounds(browser()->GetBrowserView().GetBounds());
  gfx::Rect new_bounds(original_browser_bounds);
  new_bounds.set_width(min_side_panel_width * 3);
  // Explicitly restore the browser window on ChromeOS, as it would otherwise
  // be maximized and the SetBounds call would be a no-op.
#if BUILDFLAG(IS_CHROMEOS)
  browser()->GetBrowserView().Restore();
#endif
  browser()->GetBrowserView().SetBounds(new_bounds);

  // Switch to the read anything side panel and verify the width is greater than
  // two thirds of the browser width.
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  int browser_width = browser()->GetBrowserView().GetLocalBounds().width();
  int two_thirds_browser_width = browser_width * 2 / 3;
  browser()->GetBrowserView().contents_height_side_panel()->SetPanelWidth(
      two_thirds_browser_width + 10);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_GT(browser()->GetBrowserView().contents_height_side_panel()->width(),
            two_thirds_browser_width);
}

// TODO(crbug.com/384507412): Flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ChangeSidePanelWidthNarrowWindow \
  DISABLED_ChangeSidePanelWidthNarrowWindow
#else
#define MAYBE_ChangeSidePanelWidthNarrowWindow ChangeSidePanelWidthNarrowWindow
#endif

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       MAYBE_ChangeSidePanelWidthNarrowWindow) {
  Init();
  // Set side panel to left-aligned so positive resize increments mean an
  // increase in side panel width.
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  coordinator()->DisableAnimationsForTesting();

  const int min_side_panel_width = browser()
                                       ->GetBrowserView()
                                       .contents_height_side_panel()
                                       ->GetMinimumSize()
                                       .width();

  // Set the browser width so that two thirds of the browser would be larger
  // than the minimum side panel width.
  gfx::Rect original_browser_bounds(browser()->GetBrowserView().GetBounds());
  gfx::Rect new_bounds(original_browser_bounds);
  new_bounds.set_width((min_side_panel_width - 3) * 3 / 2);
  // Explicitly restore the browser window on ChromeOS, as it would otherwise
  // be maximized and the SetBounds call would be a no-op.
#if BUILDFLAG(IS_CHROMEOS)
  browser()->GetBrowserView().Restore();
#endif
  browser()->GetBrowserView().SetBounds(new_bounds);

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  int browser_width = browser()->GetBrowserView().GetLocalBounds().width();
  int two_thirds_browser_width = browser_width * 2 / 3;
  EXPECT_GT(min_side_panel_width, two_thirds_browser_width);

  // Set the side panel width to be less than the min side panel width.
  browser()->GetBrowserView().contents_height_side_panel()->SetPanelWidth(
      min_side_panel_width - 1);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  // Verify the side panel width is the minimum width and is greater than two
  // thirds of the browser width.
  EXPECT_GT(browser()->GetBrowserView().contents_height_side_panel()->width(),
            two_thirds_browser_width);
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            min_side_panel_width);

  // Set the side panel width to be larger than the min side panel width.
  browser()->GetBrowserView().contents_height_side_panel()->SetPanelWidth(
      min_side_panel_width + 1);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  // Verify the side panel width is is the minimum width.
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            min_side_panel_width);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ChangeSidePanelWidthRightAlign) {
  Init();
  // Set side panel to right-aligned
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  const int starting_width = 500;
  browser()->GetBrowserView().contents_height_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            starting_width);

  const int increment = 50;
  browser()->GetBrowserView().contents_height_side_panel()->OnResize(increment,
                                                                     true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  // Verify positive increments reduce the side panel width
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            starting_width - increment);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidthMaxMin) {
  Init();
  // Set side panel to left-aligned so positive resize increments mean an
  // increase in side panel width.
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  const int starting_width = 500;
  auto* const side_panel =
      browser()->GetBrowserView().contents_height_side_panel();
  side_panel->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(side_panel->width(), starting_width);

  // Use an increment large enough to hit side panel and browser contents
  // minimum width constraints.
  const int large_increment = 1000000000;
  side_panel->OnResize(large_increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());

  const int browser_width =
      browser()->GetBrowserView().GetLocalBounds().width();
  const int two_thirds_browser_width = browser_width * 2 / 3;
  const int expected_width =
      std::max(two_thirds_browser_width, side_panel->GetMinimumSize().width());
  EXPECT_EQ(expected_width, side_panel->width());

  // the web contents width will either be it's min width or 1/3 the browser
  // width minus the side panel separator width.
  const int web_contents_width = std::max(
      GetMinWebContentsWidth(), (browser_width - two_thirds_browser_width - 1));
  EXPECT_EQ(browser()->GetBrowserView().contents_web_view()->width(),
            web_contents_width);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorWithSideBySideTest,
                       ChangeSidePanelWidthMaxMin) {
  Init();

  // Create split view.
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Set side panel to left-aligned so positive resize increments mean an
  // increase in side panel width.
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  const int starting_width = 500;
  browser()->GetBrowserView().contents_height_side_panel()->SetPanelWidth(
      starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->width(),
            starting_width);

  // Use an increment large enough to hit side panel and browser contents
  // minimum width constraints.
  const int large_increment = 1000000000;
  browser()->GetBrowserView().contents_height_side_panel()->OnResize(
      large_increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());

  if (base::FeatureList::IsEnabled(features::kTabbedBrowserUseNewLayout)) {
    EXPECT_EQ(browser()->GetBrowserView().multi_contents_view()->width(),
              GetMinWebContentsWidth() + views::Separator::kThickness);
  } else {
    EXPECT_EQ(browser()->GetBrowserView().multi_contents_view()->width(),
              GetMinWebContentsWidth());
    EXPECT_EQ(
        browser()->GetBrowserView().multi_contents_view()->width(),
        browser()->GetBrowserView().multi_contents_view()->GetMinViewWidth() *
            2);
  }
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
  auto* const side_panel =
      browser()->GetBrowserView().contents_height_side_panel();
  const int starting_width = 500;
  side_panel->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(side_panel->width(), starting_width);

  const int increment = 20;
  side_panel->OnResize(increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(side_panel->width(), starting_width - increment);

  // Set UI direction to RTL
  base::i18n::SetRTLForTesting(true);
  side_panel->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(side_panel->width(), starting_width);

  side_panel->OnResize(increment, true);
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  EXPECT_EQ(side_panel->width(), starting_width + increment);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ChangeSidePanelWidthWindowResize) {
  Init();
  // Wait for the side panel to be visible and fully shown.
  coordinator()->Show(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().contents_height_side_panel()->state() ==
           SidePanel::State::kOpen;
  }));

  // Set the width and wait for layout/animations.
  const int starting_width = 500;
  browser()->GetBrowserView().contents_height_side_panel()->SetPanelWidth(
      starting_width);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().contents_height_side_panel()->width() ==
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
#if BUILDFLAG(IS_CHROMEOS)
  browser()->GetBrowserView().Restore();
#endif
  browser()->GetBrowserView().SetBounds(new_bounds);
  const int min_web_contents_width = GetMinWebContentsWidth();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    // Within a couple of pixels.
    return browser()->GetBrowserView().contents_web_view()->width() <
           min_web_contents_width + 20;
  }));

  // Return browser window to original size, side panel should also return to
  // size prior to window resize.
  browser()->GetBrowserView().SetBounds(original_bounds);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().contents_height_side_panel()->width() ==
           starting_width;
  }));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelAlignment) {
  Init();
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .contents_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);
  // Toolbar height side panel should have the opposite alignment.
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .toolbar_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .contents_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);
  // Toolbar height side panel should have the opposite alignment.
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .toolbar_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);
}

// Verify that right and left alignment works the same as when in LTR mode.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ChangeSidePanelAlignmentRTL) {
  Init();
  // Forcing the language to hebrew causes the ui to enter RTL mode.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .contents_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);
  // Toolbar height side panel should have the opposite alignment.
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .toolbar_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .contents_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);
  // Toolbar height side panel should have the opposite alignment.
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .toolbar_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);
}

class SidePanelCoordinatorPanelsOnSameSideTest
    : public SidePanelCoordinatorTest {
 public:
  SidePanelCoordinatorPanelsOnSameSideTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kToolbarHeightSidePanel,
             {{
                 {features::kSidePanelRelativeAlignment.name, "same"},
             }}},
        },
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorPanelsOnSameSideTest,
                       ChangeSidePanelAlignment) {
  Init();
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .contents_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);
  // Toolbar height side panel should have the same alignment.
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .toolbar_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .contents_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);
  // Toolbar height side panel should have the same alignment.
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .toolbar_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);
}

// Verify that right and left alignment works the same as when in LTR mode.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorPanelsOnSameSideTest,
                       ChangeSidePanelAlignmentRTL) {
  Init();
  // Forcing the language to hebrew causes the ui to enter RTL mode.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .contents_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);
  // Toolbar height side panel should have the same alignment.
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .toolbar_height_side_panel()
                  ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kRight);

  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .contents_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .contents_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);
  // Toolbar height side panel should have the same alignment.
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .toolbar_height_side_panel()
                   ->IsRightAligned());
  EXPECT_EQ(browser()
                ->GetBrowserView()
                .toolbar_height_side_panel()
                ->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SidePanelToggleWithEntriesTest) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Show reading list sidepanel.
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  // Toggle reading list sidepanel to close.
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  // Toggling reading list followed by bookmarks shows the reading list side
  // panel followed by the bookmarks side panel.
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ShowOpensSidePanel) {
  Init();
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  // Verify that bookmarks is selected.
  EXPECT_EQ(GetTitleText(),
            l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ChangesTitleWhenActionItemChanges) {
  Init();
  SidePanelEntry* entry = global_registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
  EXPECT_FALSE(coordinator()->IsSidePanelShowing(entry->type()));

  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  // Bookmarks is showing and selected.
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_EQ(GetTitleText(),
            l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE));

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionSidePanelShowBookmarks,
      browser()->GetActions()->root_action_item());

  // Update the action item text.
  const std::u16string new_title = u"New Bookmarks title";
  action_item->SetText(new_title);

  // Side panel title is updated.
  EXPECT_EQ(GetTitleText(), new_title);

  // Set property to hide the title and update again.
  entry->SetProperty(kShouldShowTitleInSidePanelHeaderKey, false);
  const std::u16string ignored_title = u"Ignored title";
  action_item->SetText(ignored_title);

  // Side panel title is empty as it's not shown.
  EXPECT_EQ(GetTitleText(), u"");
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
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
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
  EXPECT_EQ(
      coordinator()->GetCurrentEntryId(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ContextualEntryDeregistered) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);

  // Verify the first tab has kShoppingInsights.
  tabs::TabInterface* tab =
      browser()->GetBrowserView().browser()->tab_strip_model()->GetTabAtIndex(
          0);
  SidePanelRegistry* registry = tab->GetTabFeatures()->side_panel_registry();
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
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);

  // Verify the first tab's registry does not have an active entry.
  tabs::TabInterface* tab =
      browser()->GetBrowserView().browser()->tab_strip_model()->GetTabAtIndex(
          0);
  SidePanelRegistry* tab_registry =
      tab->GetTabFeatures()->side_panel_registry();
  SidePanelEntryKey key(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(
      tab_registry->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
          .has_value());

  // Show an entry from the first tab's registry
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(
      tab_registry->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kShoppingInsights);

  // Deregister kShoppingInsights from the first tab.
  tab_registry->Deregister(key);

  // Verify the panel is no longer showing.
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(
      tab_registry->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
          .has_value());
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
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Verify the first tab's registry has an active entry.
  tabs::TabInterface* tab =
      browser()->GetBrowserView().browser()->tab_strip_model()->GetTabAtIndex(
          0);
  SidePanelRegistry* tab_registry =
      tab->GetTabFeatures()->side_panel_registry();
  SidePanelEntryKey key(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(
      tab_registry->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kShoppingInsights);

  // Deregister kShoppingInsights from the first tab.
  tab_registry->Deregister(key);
  EXPECT_FALSE(tab_registry->GetEntryForKey(key));

  // Verify the panel closes.
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(
      tab_registry->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
          .has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, ShowContextualEntry) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwapBetweenTwoContextualEntryWithTheSameId) {
  Init();
  // Open shopping insights for the first tab.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);

  // Switch to the second tab and open shopping insights.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadingList)));
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kShoppingInsights)));

  // Switch back to the first tab.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kShoppingInsights)));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwapBetweenTabsAfterNavigatingToContextualEntry) {
  Init();
  // Open side panel and verify it opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to a different global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to a tab where this contextual entry is not available and verify we
  // fall back to the last seen global entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadingList)));

  // Switch back to the tab where the contextual entry was visible and verify it
  // is shown.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kShoppingInsights)));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       TogglePanelWithContextualEntryShowing) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel and verify it opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntry::Id::kBookmarks)));

  // Switch to a different global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_EQ(
      coordinator()->GetCurrentEntryId(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_EQ(
      coordinator()->GetCurrentEntryId(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kShoppingInsights);

  // Close the side panel.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       TogglePanelWithGlobalEntryShowingWithTabSwitch) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel and verify it opens to kBookmarks by default.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_EQ(
      coordinator()->GetCurrentEntryId(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kShoppingInsights);

  // Switch to a global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_EQ(
      coordinator()->GetCurrentEntryId(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);

  // Close the side panel and verify the active entries are reset.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to another tab and open a contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_EQ(
      coordinator()->GetCurrentEntryId(SidePanelEntry::PanelType::kContent),
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
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_EQ(
      coordinator()->GetCurrentEntryId(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kShoppingInsights);

  // Close the side panel and verify the active entries are reset.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to another tab, open the side panel, and verify the active entries
  // are as expected.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Close the side panel and verify the active entries.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
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
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to another global entry and verify the active entry is updated.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to another tab and open a contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);

  // Close the side panel and verify active entries.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch back to the first tab and open the side panel.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch back to the second tab and verify the active entries.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Close the side panel and verify the active entries.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwitchBetweenTabWithContextualEntryAndTabWithNoEntry) {
  Init();
  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to another tab and verify the side panel is closed.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    SwitchBetweenTabWithContextualEntryAndTabWithNoEntryWhenThereIsALastActiveGlobalEntry) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  coordinator()->Toggle(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks),
                        SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to another tab and verify the side panel is closed.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwitchBackToTabWithPreviouslyVisibleContextualEntry) {
  Init();
  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to a global entry and verify the contextual entry is no longer
  // active.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to a different tab and verify state.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch back to the original tab and verify the contextual entry is not
  // active or showing.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SwitchBackToTabWithContextualEntryAfterClosingGlobal) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  // Open side panel to contextual entry and verify.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Switch to another tab and verify the side panel is closed.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Open a global entry and verify.
  coordinator()->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Verify the panel closes but the first tab still has an active entry.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());

  // Verify returning to the first tab reopens the side panel to the active
  // contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kShoppingInsights);
  EXPECT_FALSE(contextual_registries_[1]
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
}

// Verify that side panels maintain individual widths when the
// #side-panel-resizing flag is enabled. In this case, the bookmarks and reading
// list side panels should be able to have independent widths.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, SidePanelWidthPreference) {
  Init();
  ASSERT_TRUE(&browser()->GetBrowserView());
  SidePanel* side_panel =
      browser()->GetBrowserView().contents_height_side_panel();
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

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SidePanelUsesEntryDefaultWidth) {
  Init();
  coordinator()->DisableAnimationsForTesting();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SidePanel* side_panel = browser_view->contents_height_side_panel();
  ASSERT_TRUE(side_panel);

  SidePanelEntry* const bookmarks_entry = global_registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  ASSERT_TRUE(bookmarks_entry);

  // Set a custom default width for the bookmarks side panel.
  const int kTestDefaultContentWidth = 450;
  bookmarks_entry->SetDefaultContentWidthForTesting(kTestDefaultContentWidth);

  // Clear any existing preference for bookmarks.
  PrefService* prefs = browser()->profile()->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kSidePanelIdToWidth);
  update->Remove(SidePanelEntryIdToString(SidePanelEntryId::kBookmarks));

  coordinator()->Show(SidePanelEntryId::kBookmarks);
  views::test::RunScheduledLayout(browser_view);

  // Verify the custom width is used instead of the default width.
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_EQ(side_panel->width(),
            kTestDefaultContentWidth + side_panel->GetInsets().width());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SidePanelPrefOverridesEntryDefaultWidth) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SidePanel* side_panel = browser_view->contents_height_side_panel();
  ASSERT_TRUE(side_panel);

  SidePanelEntry* const bookmarks_entry = global_registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  ASSERT_TRUE(bookmarks_entry);

  const int kTestDefaultContentWidth = 450;
  const int kUserPreferredWidth = 510;
  bookmarks_entry->SetDefaultContentWidthForTesting(kTestDefaultContentWidth);

  // Set a user preference for bookmarks.
  PrefService* prefs = browser()->profile()->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kSidePanelIdToWidth);
  update->Set(SidePanelEntryIdToString(SidePanelEntryId::kBookmarks),
              base::Value(kUserPreferredWidth));

  coordinator()->Show(SidePanelEntryId::kBookmarks);
  views::test::RunScheduledLayout(browser_view);

  // Verify the side panel uses the users preferred width even if the custom
  // width is set.
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_EQ(side_panel->width(), kUserPreferredWidth);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SidePanelUsesMinimumWidthIfNoPrefOrDefault) {
  Init();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SidePanel* side_panel = browser_view->contents_height_side_panel();
  ASSERT_TRUE(side_panel);
  coordinator()->DisableAnimationsForTesting();

  // Ensure the bookmarks side panel does not have a custom default width.
  SidePanelEntry* const bookmarks_entry = global_registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  ASSERT_TRUE(bookmarks_entry);
  bookmarks_entry->SetDefaultContentWidthForTesting(
      SidePanelEntry::kSidePanelDefaultContentWidth);

  // Clear any existing preference for bookmarks.
  PrefService* prefs = browser()->profile()->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kSidePanelIdToWidth);
  update->Remove(SidePanelEntryIdToString(SidePanelEntryId::kBookmarks));

  coordinator()->Show(SidePanelEntryId::kBookmarks);
  views::test::RunScheduledLayout(browser_view);

  // Verify the bookmarks side panel defaults to the minimum size.
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_EQ(side_panel->width(), side_panel->GetMinimumSize().width());
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
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
      base::BindRepeating(
          [](SidePanelEntryScope&) { return std::make_unique<views::View>(); }),
      /*default_content_width_callback=*/base::NullCallback());
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
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
      base::BindRepeating(
          [](SidePanelEntryScope&) { return std::make_unique<views::View>(); }),
      /*default_content_width_callback=*/base::NullCallback());
  entry->AddObserver(observer.get());
  contextual_registries_[0]->Register(std::move(entry));
  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);

  // Close the side panel.
  coordinator()->Close(SidePanelEntry::PanelType::kContent);

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
      SidePanelEntry::Key(SidePanelEntry::Id::kLens),
      base::BindRepeating(
          [](int* count, SidePanelEntryScope&) {
            (*count)++;
            return std::make_unique<views::View>();
          },
          &count),
      /*default_content_width_callback=*/base::NullCallback()));
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
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));

  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
}

// Test that a crash does not occur when the browser is closed when the side
// panel view is shown but before the entry to be displayed has finished
// loading. Regression for crbug.com/1408947.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       BrowserClosedBeforeEntryLoaded) {
  Init();
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  // Allow content delays to more closely mimic real behavior.
  coordinator()->SetNoDelaysForTesting(false);
  coordinator()->Close(SidePanelEntry::PanelType::kContent);
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
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/true));

  // Switch to a tab that does not have an extension entry registered for its
  // contextual registry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(extension_key);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/false));
}

// Test that a new contextual extension entry is not shown if it's registered
// for the active tab and the global extension entry is showing.
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
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/false));
  EXPECT_FALSE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/true));

  // Give extension access to tab side panel.
  // Tab entry is not shown.
  RunSetOptions(*extension,
                /*tab_id=*/
                sessions::SessionTabHelper::IdForTab(
                    browser()->GetActiveTabInterface()->GetContents())
                    .id(),
                /*path=*/"panel.html",
                /*enabled=*/true);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/false));
  EXPECT_FALSE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/true));
}

// Test that if global or contextual entries are deregistered, and if it exists,
// the global extension entry is not shown if the active tab's extension entry
// is deregistered.
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
  GetActiveTabRegistry()->Register(CreateEntry(extension_key));
  global_registry()->Register(CreateEntry(extension_key));

  // The contextual entry should be shown.
  coordinator()->Show(extension_key);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/true));

  // If the contextual entry is deregistered while there exists a global entry,
  // the global entry is not shown.
  GetActiveTabRegistry()->Deregister(extension_key);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
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

  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/true));

  // Switch to the second tab. Since there is no active contextual/global entry
  // and no global entry with `extension_key`, the side panel should close.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(coordinator()->IsSidePanelEntryShowing(extension_key));
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
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/true));

  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/false));
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      extension_key);

  // Reset the active entry on the first tab.
  contextual_registries_[0]->ResetActiveEntryFor(
      SidePanelEntry::PanelType::kContent);

  // Switching from a tab with the global extension entry to a tab with a
  // contextual extension entry should show the contextual entry.
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/true));
  VerifyEntryExistenceAndValue(contextual_registries_[0]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
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
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(extension_key, /*for_tab=*/false));
  VerifyEntryExistenceAndValue(
      global_registry()->GetActiveEntryFor(SidePanelEntry::PanelType::kContent),
      extension_key);

  // Show shopping insights on the second tab.
  coordinator()->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights));
  // Reset the active entry on the first tab.
  contextual_registries_[0]->ResetActiveEntryFor(
      SidePanelEntry::PanelType::kContent);
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

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest, HeaderlessSidePanel) {
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
      base::BindRepeating(
          [](SidePanelEntryScope&) { return std::make_unique<views::View>(); }),
      /*default_content_width_callback=*/base::NullCallback());
  entry->set_should_show_header(false);
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);
  // Verify the side panel is showing with no header.
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_EQ(GetHeader(), nullptr);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       HeaderlessSidePanelOnTabChange) {
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));
  AddTabToBrowser(GURL("http://foo1.com"));
  AddTabToBrowser(GURL("http://foo2.com"));
  browser()->tab_strip_model()->ActivateTabAt(0);

  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
      base::BindRepeating(
          [](SidePanelEntryScope&) { return std::make_unique<views::View>(); }),
      /*default_content_width_callback=*/base::NullCallback());
  entry->set_should_show_header(false);
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  // Open the headerless side panel and verify the side panel is showing with no
  // header.
  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);
  EXPECT_FALSE(global_registry()
                   ->GetActiveEntryFor(SidePanelEntry::PanelType::kContent)
                   .has_value());
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_EQ(GetHeader(), nullptr);

  // Switch tabs and open a different side panel and verify the header is
  // showing.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  ASSERT_NE(GetHeader(), nullptr);
  EXPECT_TRUE(GetHeader()->GetVisible());

  // Verify the header is not showing if we switch back to the tab with the
  // headerless side panel open.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_EQ(GetHeader(), nullptr);
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       SidePanelPinButtonsHideInGuestMode) {
  // Check that pin button shows in normal window.
  coordinator()->SetNoDelaysForTesting(true);
  coordinator()->DisableAnimationsForTesting();
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(GetHeader()->header_pin_button()->GetVisible());

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
  EXPECT_FALSE(guest_browser->GetBrowserView()
                   .contents_height_side_panel()
                   ->GetHeaderView<SidePanelHeader>()
                   ->header_pin_button()
                   ->GetVisible());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Verifies that clicking the pin button on an extensions side panel, pins the
// extension in ToolbarActionsModel.
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ExtensionSidePanelHasPinButton) {
  Init();
  SetUpPinningTest();

  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithSidePanel("extension", std::nullopt);
  SidePanelEntryKey extension_key = GetKeyForExtension(extension->id());

  EXPECT_FALSE(coordinator()->IsSidePanelEntryShowing(extension_key));
  coordinator()->Show(extension_key);
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(extension_key));

  views::ToggleImageButton* pin_button = GetHeader()->header_pin_button();
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

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    OpeningContentsHeightSidePanelClosesToolbarHeightSidePanel) {
  // Deregister and reregister kAboutThisSite side panel with kToolbar
  // PanelType.
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
      base::BindRepeating(
          [](SidePanelEntryScope&) { return std::make_unique<views::View>(); }),
      /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  // Open the kAboutThisSite side panel and verify the toolbar height side panel
  // is showing.
  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);
  EXPECT_TRUE(
      browser()->GetBrowserView().toolbar_height_side_panel()->GetVisible());
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  // Open the kBookmarks side panel and verify the contents height side panel is
  // opened and the toolbar height side panel is closed.
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(
      browser()->GetBrowserView().toolbar_height_side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    OpeningToolbarHeightSidePanelClosesContentHeightSidePanel) {
  // Deregister and reregister kAboutThisSite side panel with kToolbar
  // PanelType.
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
      base::BindRepeating(
          [](SidePanelEntryScope&) { return std::make_unique<views::View>(); }),
      /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  // Open the kBookmarks side panel and verify the content height side panel is
  // showing.
  coordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_FALSE(
      browser()->GetBrowserView().toolbar_height_side_panel()->GetVisible());

  // Open the kAboutThisSite side panel and verify the toolbar height side panel
  // is opened and the content height side panel is closed.
  coordinator()->Show(SidePanelEntry::Id::kAboutThisSite);
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_TRUE(
      browser()->GetBrowserView().toolbar_height_side_panel()->GetVisible());
}

class SidePanelCoordinatorLensOverlayTest : public SidePanelCoordinatorTest {
 public:
  SidePanelCoordinatorLensOverlayTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({lens::features::kLensOverlay}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLensOverlayTest,
                       ShowMoreInfoButtonWhenCallbackProvided) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kLensOverlayResults);
  VerifyEntryExistenceAndValue(contextual_registries_[1]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kLensOverlayResults);
  views::ImageButton* more_info_button = GetHeader()->header_more_info_button();
  EXPECT_TRUE(more_info_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLensOverlayTest,
                       HideMoreInfoButtonWhenNoCallbackProvided) {
  Init();
  browser()->GetBrowserView().browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kLens);
  VerifyEntryExistenceAndValue(contextual_registries_[1]->GetActiveEntryFor(
                                   SidePanelEntry::PanelType::kContent),
                               SidePanelEntry::Id::kLens);
  views::ImageButton* more_info_button = GetHeader()->header_more_info_button();
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
        SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights),
        base::BindRepeating([](SidePanelEntryScope&) {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(false);
          return view;
        }),
        /*default_content_width_callback=*/base::NullCallback());
    loading_content_entry1_ = entry1.get();
    EXPECT_TRUE(global_registry()->Register(std::move(entry1)));

    // Add a kLens entry to the global registry with loading content not
    // available.
    std::unique_ptr<SidePanelEntry> entry2 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(SidePanelEntry::Id::kLens),
        base::BindRepeating([](SidePanelEntryScope&) {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(false);
          return view;
        }),
        /*default_content_width_callback=*/base::NullCallback());
    loading_content_entry2_ = entry2.get();
    EXPECT_TRUE(global_registry()->Register(std::move(entry2)));

    // Add a kAboutThisSite entry to the global registry with content available.
    std::unique_ptr<SidePanelEntry> entry3 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite),
        base::BindRepeating([](SidePanelEntryScope&) {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(true);
          return view;
        }),
        /*default_content_width_callback=*/base::NullCallback());
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
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  // A loading entry's view should be stored as the cached view and be
  // unavailable.
  views::View* loading_content = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy->IsAvailable());
  // Set the content proxy to available.
  loading_content_proxy->SetAvailable(true);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  // Switch to another entry that has loading content.
  coordinator()->Show(loading_content_entry2_->key().id());
  loading_content = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  loading_content_proxy =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy->IsAvailable());
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loading_content_entry1_->key()));
  // Set as available and make sure the title has updated.
  loading_content_proxy->SetAvailable(true);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loading_content_entry2_->key()));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLoadingContentTest,
                       TriggerSwitchToNewEntryDuringContentLoad) {
  Init();
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  coordinator()->Show(loaded_content_entry1_->key().id());
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loaded_content_entry1_->key()));

  // Switch to loading_content_entry1_ that has loading content.
  coordinator()->Show(loading_content_entry1_->key().id());
  views::View* loading_content1 = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content1, nullptr);
  SidePanelContentProxy* loading_content_proxy1 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content1);
  EXPECT_FALSE(loading_content_proxy1->IsAvailable());
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loaded_content_entry1_->key()));
  // Verify the loading_content_entry1_ is the loading entry.
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry1_->type()),
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
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry2_->type()),
      loading_content_entry2_);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loaded_content_entry1_->key()));

  // Set loading_content_entry1_ as available and verify it is not made the
  // active entry.
  loading_content_proxy1->SetAvailable(true);
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry2_->type()),
      loading_content_entry2_);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loaded_content_entry1_->key()));

  // Set loading_content_entry2_ as available and verify it is made the active
  // entry.
  loading_content_proxy2->SetAvailable(true);
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry2_->type()),
      nullptr);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loading_content_entry2_->key()));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorLoadingContentTest,
                       TriggerSwitchToCurrentVisibleEntryDuringContentLoad) {
  Init();
  coordinator()->Show(loading_content_entry1_->key().id());
  EXPECT_FALSE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());
  // A loading entry's view should be stored as the cached view and be
  // unavailable.
  views::View* loading_content = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy1 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy1->IsAvailable());
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry1_->type()),
      loading_content_entry1_);
  // Set the content proxy to available.
  loading_content_proxy1->SetAvailable(true);
  EXPECT_TRUE(
      browser()->GetBrowserView().contents_height_side_panel()->GetVisible());

  // Switch to loading_content_entry2_ that has loading content.
  coordinator()->Show(loading_content_entry2_->key().id());
  loading_content = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy2 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy2->IsAvailable());
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loading_content_entry1_->key()));
  // Verify the loading_content_entry2_ is the loading entry.
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry2_->type()),
      loading_content_entry2_);

  // While that entry is loading, switch back to the currently showing entry.
  coordinator()->Show(loading_content_entry1_->key().id());
  // Verify the loading_content_entry2_ is no longer the loading entry.
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry1_->type()),
      nullptr);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loading_content_entry1_->key()));

  // Set loading_content_entry2_ as available and verify it is not made the
  // active entry.
  loading_content_proxy2->SetAvailable(true);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loading_content_entry1_->key()));

  // Show loading_content_entry2_ and verify it shows without availability
  // needing to be set again.
  coordinator()->Show(loading_content_entry2_->key().id());
  EXPECT_EQ(
      coordinator()->GetLoadingEntryForTesting(loading_content_entry2_->type()),
      nullptr);
  EXPECT_TRUE(
      coordinator()->IsSidePanelEntryShowing(loading_content_entry2_->key()));
}

namespace {
class HidingReasonObserver : public SidePanelEntryObserver {
 public:
  HidingReasonObserver() = default;
  ~HidingReasonObserver() override = default;

  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override {
    last_entry_will_hide_reason_ = reason;
  }

  std::optional<SidePanelEntryHideReason> last_entry_will_hide_reason_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    EntryWillHideOnTabSwitchWithBackgroundedReasonToTabWithActiveSidePanel) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  HidingReasonObserver observer;
  SidePanelEntry* first_tab_entry = contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights));
  ASSERT_TRUE(first_tab_entry);
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      first_tab_observation(&observer);
  first_tab_observation.Observe(first_tab_entry);

  // Show contextual panel in first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights)));

  // Show contextual panel in second tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator()->Show(SidePanelEntry::Id::kLens);
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kLens)));

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights)));

  // Switch to the second tab and verify the hide reason.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kLens)));
  EXPECT_THAT(observer.last_entry_will_hide_reason_,
              testing::Optional(SidePanelEntryHideReason::kBackgrounded));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    EntryWillHideOnTabSwitchWithBackgroundedReasonToTabWithoutActiveSidePanel) {
  Init();
  coordinator()->DisableAnimationsForTesting();

  HidingReasonObserver observer;
  SidePanelEntry* first_tab_entry = contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights));
  ASSERT_TRUE(first_tab_entry);
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      first_tab_observation(&observer);
  first_tab_observation.Observe(first_tab_entry);

  // Show contextual panel in first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator()->Show(SidePanelEntry::Id::kShoppingInsights);
  EXPECT_TRUE(coordinator()->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights)));

  // Switch to the second tab. The panel should hide as this tab does not have
  // the contextual entry and no global entry has been shown.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(
      coordinator()->IsSidePanelShowing(SidePanelEntry::PanelType::kContent));
  EXPECT_THAT(observer.last_entry_will_hide_reason_,
              testing::Optional(SidePanelEntryHideReason::kBackgrounded));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ShowFromAnimationReparentsContentView) {
  // Deregister and reregister kAboutThisSite side panel with kToolbar
  // PanelType.
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite));
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite),
      base::BindRepeating([](SidePanelEntryScope&) {
        auto view = std::make_unique<views::View>();
        view->SetPaintToLayer();
        view->layer()->SetFillsBoundsOpaquely(false);
        return view;
      }),
      /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  auto* toolbar_height_side_panel =
      browser()->GetBrowserView().toolbar_height_side_panel();

  auto* animation_coordinator =
      toolbar_height_side_panel->animation_coordinator();
  // Set a custom container to control the animation time.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  animation_coordinator->animation_for_testing()->SetContainer(container.get());
  gfx::AnimationContainerTestApi test_api(container.get());

  coordinator()->ShowFrom(SidePanelEntryKey(SidePanelEntryId::kAboutThisSite),
                          gfx::Rect(0, 0, 100, 100));
  // Advance the animation by 150ms, at this point the browser view should own
  // the contents view.
  test_api.IncrementTime(base::Milliseconds(150));
  ASSERT_NE(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  ASSERT_EQ(
      toolbar_height_side_panel->GetContentParentView()->children().size(), 0);

  // Advance the animation to its end, at this point the contents view should be
  // reparented to the side panel's ContentParentView.
  test_api.IncrementTime(base::Milliseconds(350));
  ASSERT_EQ(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  ASSERT_EQ(
      toolbar_height_side_panel->GetContentParentView()->children().size(), 1);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    ShowFromAnimationAnimatesContentViewInTheCorrectDirection_RightAligned) {
  // Set the toolbar height side panel to be right aligned.
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  ASSERT_TRUE(browser()
                  ->GetBrowserView()
                  .toolbar_height_side_panel()
                  ->IsRightAligned());
  // Deregister and reregister kAboutThisSite side panel with kToolbar
  // PanelType.
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite));
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite),
      base::BindRepeating([](SidePanelEntryScope&) {
        auto view = std::make_unique<views::View>();
        view->SetPaintToLayer();
        view->layer()->SetFillsBoundsOpaquely(false);
        return view;
      }),
      /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  auto* toolbar_height_side_panel =
      browser()->GetBrowserView().toolbar_height_side_panel();

  auto* animation_coordinator =
      toolbar_height_side_panel->animation_coordinator();
  // Set a custom container to control the animation time.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  animation_coordinator->animation_for_testing()->SetContainer(container.get());
  gfx::AnimationContainerTestApi test_api(container.get());

  gfx::Rect browser_view_bounds = browser()->GetBrowserView().bounds();
  coordinator()->ShowFrom(
      SidePanelEntryKey(SidePanelEntryId::kAboutThisSite),
      gfx::Rect(browser_view_bounds.x(), browser_view_bounds.height() / 2,
                browser_view_bounds.width() / 4,
                browser_view_bounds.height() / 2));
  // Advance the animation by 100ms and check the x value of the animating
  // content.
  test_api.IncrementTime(base::Milliseconds(100));
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  ASSERT_NE(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  int content_x_at_first_step = browser()
                                    ->GetBrowserView()
                                    .GetBrowserViewLayoutForTesting()
                                    ->side_panel_animation_content()
                                    ->bounds()
                                    .x();

  // Advance the animation by another 100ms, and check the x value of the
  // animating content is heading in the right direction.
  test_api.IncrementTime(base::Milliseconds(100));
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  int content_x_at_second_step = browser()
                                     ->GetBrowserView()
                                     .GetBrowserViewLayoutForTesting()
                                     ->side_panel_animation_content()
                                     ->bounds()
                                     .x();

  // Verify the content is moving right.
  ASSERT_LT(content_x_at_first_step, content_x_at_second_step);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    ShowFromAnimationAnimatesContentViewInTheCorrectDirection_LeftAligned) {
  // Set the toolbar height side panel to be left aligned.
  browser()->GetBrowserView().GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  ASSERT_FALSE(browser()
                   ->GetBrowserView()
                   .toolbar_height_side_panel()
                   ->IsRightAligned());
  // Deregister and reregister kAboutThisSite side panel with kToolbar
  // PanelType.
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite));
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite),
      base::BindRepeating([](SidePanelEntryScope&) {
        auto view = std::make_unique<views::View>();
        view->SetPaintToLayer();
        view->layer()->SetFillsBoundsOpaquely(false);
        return view;
      }),
      /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  auto* toolbar_height_side_panel =
      browser()->GetBrowserView().toolbar_height_side_panel();

  auto* animation_coordinator =
      toolbar_height_side_panel->animation_coordinator();
  // Set a custom container to control the animation time.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  animation_coordinator->animation_for_testing()->SetContainer(container.get());
  gfx::AnimationContainerTestApi test_api(container.get());

  gfx::Rect browser_view_bounds = browser()->GetBrowserView().bounds();
  coordinator()->ShowFrom(
      SidePanelEntryKey(SidePanelEntryId::kAboutThisSite),
      gfx::Rect(browser_view_bounds.width() - browser_view_bounds.width() / 4,
                browser_view_bounds.height() / 2,
                browser_view_bounds.width() / 4,
                browser_view_bounds.height() / 2));
  // Advance the animation by 100ms and check the x value of the animating
  // content.
  test_api.IncrementTime(base::Milliseconds(100));
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  ASSERT_NE(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  int content_x_at_first_step = browser()
                                    ->GetBrowserView()
                                    .GetBrowserViewLayoutForTesting()
                                    ->side_panel_animation_content()
                                    ->bounds()
                                    .x();

  // Advance the animation by another 100ms, and check the x value of the
  // animating content is heading in the right direction.
  test_api.IncrementTime(base::Milliseconds(100));
  views::test::RunScheduledLayout(&browser()->GetBrowserView());
  int content_x_at_second_step = browser()
                                     ->GetBrowserView()
                                     .GetBrowserViewLayoutForTesting()
                                     ->side_panel_animation_content()
                                     ->bounds()
                                     .x();

  // Verify the content is moving left.
  ASSERT_GT(content_x_at_first_step, content_x_at_second_step);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorTest,
                       ClosingMidShowFromAnimationReparentsContentView) {
  // Deregister and reregister kAboutThisSite side panel with kToolbar
  // PanelType.
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite));
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite),
      base::BindRepeating([](SidePanelEntryScope&) {
        auto view = std::make_unique<views::View>();
        view->SetPaintToLayer();
        view->layer()->SetFillsBoundsOpaquely(false);
        return view;
      }),
      /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(entry));
  coordinator()->SetNoDelaysForTesting(true);

  auto* toolbar_height_side_panel =
      browser()->GetBrowserView().toolbar_height_side_panel();

  auto* animation_coordinator =
      toolbar_height_side_panel->animation_coordinator();
  // Set a custom container to control the animation time.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  animation_coordinator->animation_for_testing()->SetContainer(container.get());
  gfx::AnimationContainerTestApi test_api(container.get());

  coordinator()->ShowFrom(SidePanelEntryKey(SidePanelEntryId::kAboutThisSite),
                          gfx::Rect(0, 0, 100, 100));
  // Advance the animation by 150ms, at this point the browser view should own
  // the contents view.
  test_api.IncrementTime(base::Milliseconds(150));
  ASSERT_NE(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  ASSERT_EQ(
      toolbar_height_side_panel->GetContentParentView()->children().size(), 0);

  // Trigger the side panel to close, at this point the contents view should be
  // reparented to the side panel's ContentParentView.
  coordinator()->Close(SidePanelEntry::PanelType::kToolbar,
                       SidePanelEntryHideReason::kSidePanelClosed,
                       /*suppress_animations=*/false);
  ASSERT_EQ(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  ASSERT_EQ(
      toolbar_height_side_panel->GetContentParentView()->children().size(), 1);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorTest,
    ShowFromAnimationReparentsContentViewIfInterruptedByADifferentShowAnimation) {
  // Deregister and reregister kAboutThisSite side panel with kToolbar
  // PanelType.
  global_registry()->Deregister(
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite));
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  std::unique_ptr<SidePanelEntry> entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntryId::kAboutThisSite),
      base::BindRepeating([](SidePanelEntryScope&) {
        auto view = std::make_unique<views::View>();
        view->SetPaintToLayer();
        view->layer()->SetFillsBoundsOpaquely(false);
        return view;
      }),
      /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(entry));

  // Deregister and reregister kShoppingInsights side panel with kToolbar
  // PanelType.
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntryId::kShoppingInsights));
  std::unique_ptr<SidePanelEntry> shopping_entry =
      std::make_unique<SidePanelEntry>(
          SidePanelEntry::PanelType::kToolbar,
          SidePanelEntry::Key(SidePanelEntryId::kShoppingInsights),
          base::BindRepeating([](SidePanelEntryScope&) {
            auto view = std::make_unique<views::View>();
            view->SetPaintToLayer();
            view->layer()->SetFillsBoundsOpaquely(false);
            return view;
          }),
          /*default_content_width_callback=*/base::NullCallback());
  registry->Register(std::move(shopping_entry));
  coordinator()->SetNoDelaysForTesting(true);

  auto* toolbar_height_side_panel =
      browser()->GetBrowserView().toolbar_height_side_panel();

  auto* animation_coordinator =
      toolbar_height_side_panel->animation_coordinator();
  // Set a custom container to control the animation time.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  animation_coordinator->animation_for_testing()->SetContainer(container.get());
  gfx::AnimationContainerTestApi test_api(container.get());

  coordinator()->ShowFrom(SidePanelEntryKey(SidePanelEntryId::kAboutThisSite),
                          gfx::Rect(0, 0, 100, 100));
  // Advance the animation by 150ms, at this point the browser view should own
  // the contents view.
  test_api.IncrementTime(base::Milliseconds(150));
  ASSERT_NE(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  ASSERT_EQ(
      toolbar_height_side_panel->GetContentParentView()->children().size(), 0);

  // Show the kShoppingInsights side panel mid content transition animation and
  // verify the content is correctly reparented.
  coordinator()->Show(SidePanelEntryId::kShoppingInsights);
  ASSERT_EQ(browser()
                ->GetBrowserView()
                .GetBrowserViewLayoutForTesting()
                ->side_panel_animation_content(),
            nullptr);
  ASSERT_EQ(
      toolbar_height_side_panel->GetContentParentView()->children().size(), 1);
}
