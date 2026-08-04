// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_utils.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"
#include "url/gurl.h"

namespace {

class TestLocationBarObserver : public LocationBar::Observer {
 public:
  explicit TestLocationBarObserver(base::OnceClosure on_bounds_changed)
      : on_bounds_changed_(std::move(on_bounds_changed)) {}

  void OnLocationBarBoundsChanged() override {
    ASSERT_FALSE(on_bounds_changed_.is_null());
    std::move(on_bounds_changed_).Run();
  }

 private:
  base::OnceClosure on_bounds_changed_;
};

class WebUILocationBarBrowserTest : public InProcessBrowserTest {
 public:
  WebUILocationBarBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar},
        {});
  }

  LocationBar* GetLocationBar() {
    return BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();
  }

  WebUIToolbarWebView* GetWebUIToolbarWebView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetWebUIToolbarViewForTesting();
  }

  content::WebContents* GetWebUIToolbarWebContents() {
    return GetWebUIToolbarWebView()->web_contents();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, GetAnchor) {
  WaitForInitialWebUIToolbar(browser());

  auto* location_bar = GetLocationBar();

  // Wait until visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return location_bar->GetAnchorOrNull(); }));
  ui::TrackedElement* anchor = location_bar->GetAnchorOrNull();
  ASSERT_TRUE(anchor);
  ASSERT_TRUE(anchor->IsA<ui::TrackedElementWebUI>());
  gfx::Rect location_bar_rect = anchor->GetScreenBounds();
  // Check that the anchor is the expected height, and closely to the right of
  // the reload button, with the expected margin.
  EXPECT_EQ(location_bar_rect.height(),
            GetLayoutConstant(LayoutConstant::kLocationBarHeight))
      << location_bar_rect.ToString();

  ui::TrackedElement* reload =
      BrowserElements::From(browser())->GetElement(kReloadButtonElementId);
  gfx::Rect reload_rect = reload->GetScreenBounds();
  EXPECT_EQ(location_bar_rect.x() - reload_rect.right(),
            GetLayoutConstant(LayoutConstant::kLocationBarMargin))
      << location_bar_rect.ToString() << " " << reload_rect.ToString();
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, Accessors) {
  auto* location_bar = GetLocationBar();
  EXPECT_EQ(browser(), location_bar->GetBrowser());
  EXPECT_EQ(browser()->GetProfile(), location_bar->GetProfile());
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, Bounds) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* location_bar = GetLocationBar();

  WaitForInitialWebUIToolbar(browser());

  // Wait until visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return location_bar->GetAnchorOrNull(); }));

  // Since GetAnchor() is tested, we can partly rely on it.
  gfx::Rect screen_bounds = location_bar->BoundsInScreen();
  EXPECT_EQ(screen_bounds, location_bar->GetAnchorOrNull()->GetScreenBounds());
  // Also should be kinda wide.
  EXPECT_GT(screen_bounds.width(), browser_view->width() / 2);

  gfx::Rect relative_bounds = location_bar->Bounds();
  EXPECT_EQ(relative_bounds.size(), screen_bounds.size());

  ToolbarButtonProvider* toolbar_button_provider = browser_view->toolbar();
  auto* webview = toolbar_button_provider->GetWebUIToolbarViewForTesting();

  gfx::Vector2d offset =
      screen_bounds.origin() - webview->GetBoundsInScreen().origin();
  EXPECT_EQ(offset.x(), relative_bounds.x());
  EXPECT_EQ(offset.y(), relative_bounds.y());

  // Make sure that bounds change observer gets notified.
  base::RunLoop run_loop;
  TestLocationBarObserver bounds_observer(run_loop.QuitClosure());
  base::ScopedObservation<LocationBar, LocationBar::Observer> obs(
      &bounds_observer);
  obs.Observe(location_bar);
  browser_view->SetSize(
      gfx::Size(browser_view->width() - 100, browser_view->height()));
  run_loop.Run();
}

// Test that basic state management of the omnibox works --- e.g. it gets
// the URL as its state when navigating and switching tabs.
IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, BasicOmniboxState) {
  WaitForInitialWebUIToolbar(browser());
  LocationBar* location_bar = GetLocationBar();
  auto* tab_strip_model = browser()->tab_strip_model();

  auto* omnibox = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox);
  EXPECT_EQ("about:blank", base::UTF16ToUTF8(omnibox->GetText()));

  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  tab_strip_model->SelectTabAt(1);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));
  EXPECT_EQ("chrome://version", base::UTF16ToUTF8(omnibox->GetText()));

  tab_strip_model->SelectTabAt(0);
  EXPECT_EQ("about:blank", base::UTF16ToUTF8(omnibox->GetText()));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://chrome-urls")));
  EXPECT_EQ("chrome://chrome-urls", base::UTF16ToUTF8(omnibox->GetText()));

  tab_strip_model->SelectTabAt(1);
  EXPECT_EQ("chrome://version", base::UTF16ToUTF8(omnibox->GetText()));
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, LocationIcon) {
  WaitForInitialWebUIToolbar(browser());
  LocationBar* location_bar = GetLocationBar();
  auto* omnibox = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox);
  EXPECT_EQ("about:blank", base::UTF16ToUTF8(omnibox->GetText()));

  const char kGetIcon[] = R"(
      document.querySelector('toolbar-app')?.
        shadowRoot?.querySelector('location-bar')?.
        shadowRoot?.querySelector('location-icon')?.
        shadowRoot?.querySelector('icon-from-table')?.
        shadowRoot?.querySelector('cr-icon')?.
        icon;
    )";

  EXPECT_EQ("webui-toolbar:info",
            content::EvalJs(GetWebUIToolbarWebContents(), kGetIcon));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));
  EXPECT_EQ("chrome://version", base::UTF16ToUTF8(omnibox->GetText()));

  EXPECT_EQ("webui-toolbar:chrome_product",
            content::EvalJs(GetWebUIToolbarWebContents(), kGetIcon));
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, PageActionNavigation) {
  WaitForInitialWebUIToolbar(browser());
  auto* location_bar = static_cast<WebUILocationBar*>(GetLocationBar());
  auto& control = location_bar->page_action_control();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  auto* controller = tab->GetTabFeatures()->page_action_controller();

  controller->Show(kActionAiMode);
  EXPECT_FALSE(control.GetPageActionStates().empty());

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  tab = tabs::TabInterface::GetFromContents(web_contents);
  controller = tab->GetTabFeatures()->page_action_controller();

  controller->Show(kActionAiMode);
  EXPECT_FALSE(control.GetPageActionStates().empty());
}

// Display all available page actions and check that a button is rendered.
IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, AllPageActionsPresent) {
  WaitForInitialWebUIToolbar(browser());
  auto* location_bar = static_cast<WebUILocationBar*>(GetLocationBar());
  auto& control = location_bar->page_action_control();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  auto* controller = tab->GetTabFeatures()->page_action_controller();

  constexpr char kGetPageActionIconsScript[] = R"(
    Array.from(document.querySelector('toolbar-app')?.
      shadowRoot?.querySelector('location-bar')?.
      shadowRoot?.querySelector('page-action-icons')?.
      shadowRoot?.querySelectorAll('page-action-icon') || [])
  )";

  for (actions::ActionId action_id : page_actions::kActionIds) {
    SCOPED_TRACE(action_id);
    if (!controller->ActionExists(action_id)) {
      continue;
    }
    // Enable and make the action item visible.
    actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
        action_id, BrowserActions::From(browser())->root_action_item());
    if (action_item) {
      action_item->SetVisible(true);
      action_item->SetEnabled(true);
      // Add a static vector image if the action item has none, except leave the
      // fake one without an image.
      if (action_item->GetImage().IsEmpty() &&
          action_id != kActionFakePageActionForDebug) {
        action_item->SetImage(
            ui::ImageModel::FromVectorIcon(vector_icons::kFeedbackIcon));
      }
    }
    controller->Show(action_id);
    EXPECT_FALSE(control.GetPageActionStates().empty());

    // Verify the button appears.
    const bool is_fake_action = (action_id == kActionFakePageActionForDebug);
    const int mojom_id =
        static_cast<int>(webui_toolbar::ActionIdToMojomPageActionId(action_id));
    const std::string kCheckButtonJs =
        content::JsReplace(base::StrCat({kGetPageActionIconsScript, R"(
            .some(icon => {
              if (!icon.state || icon.state.pageActionId !== $1) {
                return false;
              }
              const button = icon.shadowRoot?.querySelector('cr-icon-button');
              if (!button) {
                return false;
              }
              if (icon.state.icon.handleId == 0) {
                return $2;
              }
              if (!button.hasAttribute('iron-icon')) {
                return false;
              }
              const crIcons =
                  button.shadowRoot?.querySelectorAll('#icon cr-icon') || [];
              if (crIcons.length === 0) {
                return false;
              }
              for (const crIcon of crIcons) {
                if (!crIcon.shadowRoot?.querySelector('svg')) {
                  return false;
                }
              }
              return true;
            });
        )"}),
                           mojom_id, is_fake_action);

    EXPECT_TRUE(base::test::RunUntil([&]() {
      // While we're waiting, the page action can be hidden by other code, e.g.
      // AiModePageActionController and ZoomViewController, so continue to
      // request it be shown. Show() dedups requests so there shouldn't be a
      // cost to re-requesting.
      controller->Show(action_id);
      return content::EvalJs(GetWebUIToolbarWebContents(), kCheckButtonJs)
          .ExtractBool();
    })) << "Failed to find page action button in HTML for action id: "
        << action_id;

    controller->Hide(action_id);
    const std::string kCheckButtonHiddenJs =
        content::JsReplace(base::StrCat({"!", kGetPageActionIconsScript, R"(
            .some(icon => icon.state && icon.state.pageActionId === $1);
        )"}),
                           mojom_id);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(GetWebUIToolbarWebContents(), kCheckButtonHiddenJs)
          .ExtractBool();
    })) << "Failed to hide page action button in HTML for action id: "
        << action_id;
  }
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest,
                       ContentSettingIconAnimation) {
  WaitForInitialWebUIToolbar(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Block content on active WebContents to trigger content setting icons.
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  content_settings->BlockAllContentForTesting();

  // Update location bar so state is propagated to WebUI.
  GetLocationBar()->Update(web_contents);

  constexpr char kCheckIconScript[] = R"(
      (() => {
        const icons = Array.from(
          document.querySelector('toolbar-app')?.
            shadowRoot?.querySelector('location-bar')?.
            shadowRoot?.querySelector('content-settings-icons')?.
            shadowRoot?.querySelectorAll('content-setting-icon') || []
        );
        return icons.length > 0;
      })()
  )";

  EXPECT_TRUE(base::test::RunUntil([&]() {
    GetLocationBar()->Update(web_contents);
    return content::EvalJs(GetWebUIToolbarWebContents(), kCheckIconScript)
        .ExtractBool();
  }));

  // Trigger a second update on the same web contents.
  GetLocationBar()->Update(web_contents);

  // Verify that icons exist and animation state is stable.
  EXPECT_TRUE(content::EvalJs(GetWebUIToolbarWebContents(), kCheckIconScript)
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest,
                       StarredPageActionIconColor) {
  WaitForInitialWebUIToolbar(browser());

  auto* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  page_actions::WebUIPageActionControl control(
      BrowserActions::From(browser())->root_action_item());
  control.Init(GetWebUIToolbarWebView());
  control.UpdateController(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* bookmark_controller = BookmarkPageActionController::From(tab);
  ASSERT_TRUE(bookmark_controller);

  // Set unstarred:
  bookmark_controller->URLStarredChanged(
      browser()->tab_strip_model()->GetActiveWebContents(), /*starred=*/false);
  auto states_unstarred = control.GetPageActionStates();
  auto it_unstarred = std::find_if(
      states_unstarred.begin(), states_unstarred.end(), [](const auto& state) {
        return state->page_action_id ==
               toolbar_ui_api::mojom::PageActionId::kActionBookmarkThisTab;
      });
  ASSERT_NE(it_unstarred, states_unstarred.end());

  auto fetcher_unstarred = GetWebUIToolbarWebView()->GetIconTableFetcher();
  auto full_state_unstarred = fetcher_unstarred->GetFullState();
  auto icon_update_unstarred = std::find_if(
      full_state_unstarred.begin(), full_state_unstarred.end(),
      [&](const toolbar_ui_api::mojom::IconUpdatePtr& update) {
        return update->handle_id == (*it_unstarred)->icon.HandleId().value();
      });
  EXPECT_TRUE((*icon_update_unstarred)->color.has_value());

  // Set starred:
  bookmark_controller->URLStarredChanged(
      browser()->tab_strip_model()->GetActiveWebContents(), /*starred=*/true);
  auto states_starred = control.GetPageActionStates();
  auto it_starred = std::find_if(
      states_starred.begin(), states_starred.end(), [](const auto& state) {
        return state->page_action_id ==
               toolbar_ui_api::mojom::PageActionId::kActionBookmarkThisTab;
      });
  ASSERT_NE(it_starred, states_starred.end());

  auto fetcher_starred = GetWebUIToolbarWebView()->GetIconTableFetcher();
  auto full_state_starred = fetcher_starred->GetFullState();
  auto icon_update_starred = std::find_if(
      full_state_starred.begin(), full_state_starred.end(),
      [&](const toolbar_ui_api::mojom::IconUpdatePtr& update) {
        return update->handle_id == (*it_starred)->icon.HandleId().value();
      });
  ASSERT_NE(icon_update_starred, full_state_starred.end());
  const SkColor expected_color =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetColorProvider()
          ->GetColor(ui::kColorFocusableBorderFocused);
  EXPECT_EQ((*icon_update_starred)->color, expected_color);
  EXPECT_NE((*icon_update_unstarred)->color, (*icon_update_starred)->color);
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest,
                       StarredPageActionLowContrastBlending) {
  // Override ColorProvider to simulate a low-contrast custom theme where both
  // the toolbar background (`kColorToolbar`) and the accent color
  // (`ui::kColorFocusableBorderFocused`) are set to identical blue, resulting
  // in an initial contrast ratio of 1:1.
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(
          [](ui::ColorProvider* provider, const ui::ColorProviderKey& key) {
            ui::ColorMixer& mixer = provider->AddMixer();
            mixer[kColorToolbar] = {SK_ColorBLUE};
            mixer[ui::kColorFocusableBorderFocused] = {SK_ColorBLUE};
          }));
  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->ThemeChanged();

  auto* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  BookmarkPageActionController::From(tab)->URLStarredChanged(
      browser()->tab_strip_model()->GetActiveWebContents(), /*starred=*/true);

  page_actions::WebUIPageActionControl control(
      BrowserActions::From(browser())->root_action_item());
  control.Init(GetWebUIToolbarWebView());
  control.UpdateController(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto states = control.GetPageActionStates();
  auto it = std::find_if(states.begin(), states.end(), [](const auto& state) {
    return state->page_action_id ==
           toolbar_ui_api::mojom::PageActionId::kActionBookmarkThisTab;
  });
  ASSERT_NE(it, states.end());

  auto full_state =
      GetWebUIToolbarWebView()->GetIconTableFetcher()->GetFullState();
  auto icon_update =
      std::find_if(full_state.begin(), full_state.end(),
                   [&](const toolbar_ui_api::mojom::IconUpdatePtr& update) {
                     return update->handle_id == (*it)->icon.HandleId().value();
                   });
  ASSERT_NE(icon_update, full_state.end());
  ASSERT_TRUE((*icon_update)->color.has_value());

  // Verify that `color_utils::BlendForMinContrast()` automatically adjusted
  // the icon color so it is no longer identical to `SK_ColorBLUE`, ensuring
  // the contrast against the toolbar background meets or exceeds the 3.0:1
  // threshold.
  const SkColor actual_color = (*icon_update)->color.value();
  EXPECT_NE(actual_color, SK_ColorBLUE);
  EXPECT_GE(color_utils::GetContrastRatio(actual_color, SK_ColorBLUE),
            color_utils::kMinimumVisibleContrastRatio);
}

}  // namespace
