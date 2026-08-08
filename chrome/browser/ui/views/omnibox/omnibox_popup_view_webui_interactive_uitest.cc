// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/permissions/test/mock_permission_request.h"
#if BUILDFLAG(IS_MAC)
#include "components/viz/common/features.h"
#endif
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_getter.h"
#endif

// ChromeOS environment doesn't instantiate the NewWebUI<OmniboxPopupUI>
// in the factory's GetWebUIFactoryFunction, so these don't work there yet.
// Also avoid burdening test bots on mobile platforms where webui omnibox
// isn't ready and the platform-specific views implementation is in scope.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Base class for omnibox browser and ui tests.
class OmniboxPopupViewWebUITest : public InProcessBrowserTest {
 public:
  OmniboxPopupViewWebUITest();
  ~OmniboxPopupViewWebUITest() override;
  OmniboxPopupViewWebUITest(const OmniboxPopupViewWebUITest&) = delete;
  OmniboxPopupViewWebUITest& operator=(const OmniboxPopupViewWebUITest&) =
      delete;

  // Helper to wait for theme changes. The wait is triggered when an instance of
  // this class goes out of scope.
  class ThemeChangeWaiter {
   public:
    explicit ThemeChangeWaiter(ThemeService* theme_service)
        : waiter_(theme_service) {}
    ThemeChangeWaiter(const ThemeChangeWaiter&) = delete;
    ThemeChangeWaiter& operator=(const ThemeChangeWaiter&) = delete;
    ~ThemeChangeWaiter();

   private:
    test::ThemeServiceChangedWaiter waiter_;
  };

  void CreatePopupForTestQuery();

  LocationBarView* location_bar() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar_view();
  }
  OmniboxViewViews* omnibox_view() { return location_bar()->omnibox_view(); }
  OmniboxController* controller() {
    return location_bar()->GetOmniboxController();
  }
  OmniboxEditModel* edit_model() {
    return location_bar()->GetOmniboxController()->edit_model();
  }

  SkColor GetSelectedColor(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->GetColorProvider()
        ->GetColor(kColorOmniboxResultsBackgroundSelected);
  }

  SkColor GetNormalColor(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->GetColorProvider()
        ->GetColor(kColorOmniboxResultsBackground);
  }

  // Some tests relies on the light/dark variants of the result background to be
  // different. But when using the system theme on Linux, these colors will be
  // the same. Ensure we're not using the system theme, which may be
  // conditionally enabled depending on the environment.
  void UseDefaultTheme();

  OmniboxTriggeredFeatureService* triggered_feature_service() {
    return &triggered_feature_service_;
  }

  void SetUp() override;

 private:
  OmniboxTriggeredFeatureService triggered_feature_service_;
  base::test::ScopedFeatureList feature_list_;
};

OmniboxPopupViewWebUITest::OmniboxPopupViewWebUITest() {
  feature_list_.InitAndEnableFeature(omnibox::internal::kWebUIOmniboxPopup);
}
OmniboxPopupViewWebUITest::~OmniboxPopupViewWebUITest() = default;

OmniboxPopupViewWebUITest::ThemeChangeWaiter::~ThemeChangeWaiter() {
  waiter_.WaitForThemeChanged();
  // Theme changes propagate asynchronously in DesktopWindowTreeHostX11::
  // FrameTypeChanged(), so ensure all tasks are consumed.
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).Run();
}

void OmniboxPopupViewWebUITest::CreatePopupForTestQuery() {
  auto* autocomplete_controller = controller()->autocomplete_controller();
  EXPECT_TRUE(autocomplete_controller->result().empty());
  EXPECT_FALSE(controller()->IsPopupOpen());

  // Verify that the popup state manager callback is called when popup opens.
  UNCALLED_MOCK_CALLBACK(
      base::RepeatingCallback<void(OmniboxPopupState, OmniboxPopupState)>,
      popup_callback);
  const auto subscription =
      controller()->popup_state_manager()->AddPopupStateChangedCallback(
          popup_callback.Get());

  EXPECT_CALL_IN_SCOPE(
      popup_callback,
      Run(OmniboxPopupState::kNone, OmniboxPopupState::kClassic), {
        edit_model()->SetUserText(u"foo");
        AutocompleteInput input(
            u"foo", metrics::OmniboxEventProto::BLANK,
            ChromeAutocompleteSchemeClassifier(browser()->GetProfile()));
        input.set_omit_asynchronous_matches(true);
        autocomplete_controller->Start(input);

        EXPECT_FALSE(autocomplete_controller->result().empty());
        EXPECT_TRUE(controller()->IsPopupOpen());
      });
}

void OmniboxPopupViewWebUITest::UseDefaultTheme() {
#if BUILDFLAG(IS_LINUX)
  // Normally it would be sufficient to call ThemeService::UseDefaultTheme()
  // which sets the kUsesSystemTheme user pref on the browser's profile.
  // However BrowserThemeProvider::GetColorProviderColor() currently does not
  // pass an aura::Window to LinuxUI::GetNativeTheme() - which means that the
  // NativeThemeGtk instance will always be returned.
  // TODO(crbug.com/40217733): Remove this once GTK passthrough is fully
  // supported.
  ui::LinuxUiGetter::set_instance(nullptr);
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();

  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->GetProfile());
  if (!theme_service->UsingDefaultTheme()) {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseDefaultTheme();
  }
  ASSERT_TRUE(theme_service->UsingDefaultTheme());
#endif  // BUILDFLAG(IS_LINUX)
}

void OmniboxPopupViewWebUITest::SetUp() {
  InProcessBrowserTest::SetUp();
}



// Check that the location bar background (and the background of the textfield
// it contains) changes when it receives focus, and matches the popup background
// color.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUITest,
                       PopupMatchesLocationBarBackground) {
  // In dark mode the omnibox focused and unfocused colors are the same, which
  // makes this test fail; see comments below.
  ui::MockOsSettingsProvider os_settings_provider;  // Forces light mode.

  // Start with the Omnibox unfocused.
  omnibox_view()->GetFocusManager()->ClearFocus();
  const SkColor color_before_focus =
      location_bar()->GetBackgroundColorForTesting();
  EXPECT_EQ(color_before_focus, omnibox_view()->GetBackgroundColor());

  // Give the Omnibox focus and get its focused color.
  omnibox_view()->RequestFocus();
  const SkColor color_after_focus =
      location_bar()->GetBackgroundColorForTesting();

  EXPECT_EQ(color_after_focus, omnibox_view()->GetBackgroundColor());

  // The background is hosted in the view that contains the results area.
  CreatePopupForTestQuery();
  LocationBarView* background_host = location_bar();
  EXPECT_EQ(color_after_focus, background_host->GetBackgroundColorForTesting());

  omnibox_view()->GetFocusManager()->ClearFocus();

  // Blurring the Omnibox w/ in-progress input (e.g. "foo") should result in
  // the on-focus colors.
  EXPECT_EQ(color_after_focus, location_bar()->GetBackgroundColorForTesting());
  EXPECT_EQ(color_after_focus, omnibox_view()->GetBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUITest, PopupLoadsAndAcceptsCalls) {
  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  popup_view->presenter()->Show();
  popup_view->UpdatePopupAppearance();
  OmniboxPopupSelection selection(OmniboxPopupSelection::kNoMatch);
  popup_view->ProvideButtonFocusHint(0);
  popup_view->presenter()->Hide();
}

class OmniboxPopupViewWebUIFullV2Test : public OmniboxPopupViewWebUITest {
 public:
  OmniboxPopupViewWebUIFullV2Test() {
    // interactive_ui_tests sets `ui_test_utils::BringBrowserWindowToFront()`
    // for the setup function by default, which causes timeouts on Windows bots.
    // Unset it here as this test does not strictly require the window to be in
    // front to verify state isolation.
    set_global_browser_set_up_function(nullptr);
    feature_list_full_v2_.InitWithFeatures(
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::kWebUIOmniboxFullPopup},
        {});
  }

  std::unique_ptr<views::Widget> DeactivatePopupWidget() {
    // Wait for the popup transition to complete (clearing
    // in_popup_state_transition).
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

    // Since in background test runners the browser window is not natively
    // active, focusing on the web contents will not activate the browser
    // widget. Create a temporary dummy widget and activate it to force the main
    // browser window (and the child popup widget) inactive.
    auto dummy_widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.context = location_bar()->GetWidget()->GetNativeWindow();
    params.bounds = gfx::Rect(0, 0, 100, 100);
    dummy_widget->Init(std::move(params));
    dummy_widget->Show();
    dummy_widget->Activate();

    // In background test runners, the OS blocks background applications from
    // changing native window activation.
    // Directly notify the presenter observer of focus loss.
    auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
        location_bar()->GetOmniboxPopupView());
    auto* presenter =
        static_cast<OmniboxPopupFullPresenter*>(popup_view->presenter());
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        presenter->get_widget_for_testing(), /*active=*/false);

    return dummy_widget;
  }

 private:
  base::test::ScopedFeatureList feature_list_full_v2_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUIFullV2Test, TabSwitchStateSync) {
  // Create a new tab.
  int initial_tab_index = browser()->tab_strip_model()->active_index();
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  int new_tab_index = browser()->tab_strip_model()->active_index();
  ASSERT_NE(initial_tab_index, new_tab_index);
  // Type text in the omnibox of the active tab (new tab) and select it.
  omnibox_view()->SetUserText(u"test query");
  omnibox_view()->SelectAll(false);
  gfx::Range initial_selection = omnibox_view()->GetSelectionBounds();

  // Directly notify the WebUI popup handler of the input state and selection
  // bounds. This ensures the backend state is cached and ready to be restored
  // or reset when switching tabs.
  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  auto* webui_controller = popup_view->presenter()
                               ->GetWebUIContent()
                               ->contents_wrapper()
                               ->GetWebUIController();
  auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller);
  if (auto* popup_handler = popup_ui ? popup_ui->popup_handler() : nullptr) {
    popup_handler->OnSelectionChanged(initial_selection, 10000, false);
  }

  // Switch to another tab (initial tab).
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_index);
  // Verify the text is isolated (not the typed text) in the other tab.
  EXPECT_NE(u"test query", omnibox_view()->GetText());
  // Switch back to the original tab (new tab).
  browser()->tab_strip_model()->ActivateTabAt(new_tab_index);
  // Verify the text and selection are restored.
  EXPECT_EQ(u"test query", omnibox_view()->GetText());
  auto* popup_view_check = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  auto* webui_controller_check = popup_view_check->presenter()
                                     ->GetWebUIContent()
                                     ->contents_wrapper()
                                     ->GetWebUIController();
  auto* popup_ui_check = static_cast<OmniboxPopupUI*>(webui_controller_check);
  if (auto* popup_handler_check =
          popup_ui_check ? popup_ui_check->popup_handler() : nullptr) {
    EXPECT_EQ(initial_selection, popup_handler_check->latest_selection());
  }
}

// TODO(crbug.com/536046012): Re-enable this test on Linux and Mac.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_TabSwitchNoSavedState DISABLED_TabSwitchNoSavedState
#else
#define MAYBE_TabSwitchNoSavedState TabSwitchNoSavedState
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUIFullV2Test, MAYBE_TabSwitchNoSavedState) {
  // Create a new tab.
  int initial_tab_index = browser()->tab_strip_model()->active_index();
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Focus the location bar to ensure the Omnibox has active focus.
  location_bar()->FocusLocation(/*is_user_initiated=*/true,
                                /*clear_focus_if_failed=*/false);

  // Type text in the omnibox of the active tab (new tab) and select it.
  omnibox_view()->SetUserText(u"test query");
  omnibox_view()->SelectAll(false);
  gfx::Range initial_selection = omnibox_view()->GetSelectionBounds();

  // Directly notify the WebUI popup handler of the input state and selection
  // bounds. This ensures the backend state is cached and ready to be restored
  // or reset when switching tabs.
  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  auto* webui_controller = popup_view->presenter()
                               ->GetWebUIContent()
                               ->contents_wrapper()
                               ->GetWebUIController();
  auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller);
  if (auto* popup_handler = popup_ui ? popup_ui->popup_handler() : nullptr) {
    popup_handler->OnSelectionChanged(initial_selection, 10000, false);
  }

  // Clear any saved omnibox state from the initial tab.
  content::WebContents* initial_contents =
      browser()->tab_strip_model()->GetWebContentsAt(initial_tab_index);
  initial_contents->RemoveUserData(OmniboxTabHelper::kOmniboxStateKey);

  // Switch back to the initial tab.
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_index);

  // Verify the selection matches the focus state when activating a tab with
  // no saved state.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto* popup_view_check = static_cast<OmniboxPopupViewWebUI*>(
        location_bar()->GetOmniboxPopupView());
    if (!popup_view_check || !popup_view_check->presenter() ||
        !popup_view_check->presenter()->GetWebUIContent() ||
        !popup_view_check->presenter()->GetWebUIContent()->contents_wrapper()) {
      return false;
    }
    auto* webui_controller_check = popup_view_check->presenter()
                                       ->GetWebUIContent()
                                       ->contents_wrapper()
                                       ->GetWebUIController();
    auto* popup_ui_check = static_cast<OmniboxPopupUI*>(webui_controller_check);
    auto* popup_handler_check =
        popup_ui_check ? popup_ui_check->popup_handler() : nullptr;
    const gfx::Range expected_selection = gfx::Range(0, 0);
    return popup_handler_check &&
           popup_handler_check->latest_selection() == expected_selection;
  }));
}

// TODO(crbug.com/542637306): Re-enable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DeactivationClearsPopupState DISABLED_DeactivationClearsPopupState
#else
#define MAYBE_DeactivationClearsPopupState DeactivationClearsPopupState
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUIFullV2Test,
                       MAYBE_DeactivationClearsPopupState) {
  // Focus the location bar to ensure the Omnibox has active focus and the popup
  // is open.
  location_bar()->FocusLocation(/*is_user_initiated=*/true,
                                /*clear_focus_if_failed=*/false);

  // Verify that the popup state is initially kFull.
  ASSERT_EQ(OmniboxPopupState::kFull,
            controller()->popup_state_manager()->popup_state());

  auto dummy_widget = DeactivatePopupWidget();

  // Wait for the deferred deactivation task to run and verify that the popup
  // state transitions to kNone.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return controller()->popup_state_manager()->popup_state() ==
           OmniboxPopupState::kNone;
  }));
}

// TODO(crbug.com/536046012): Re-enable this test.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUIFullV2Test,
                       DISABLED_DeactivationWithTextDoesNotClearPopupState) {
  // Focus the location bar to ensure the Omnibox has active focus and the popup
  // is open.
  location_bar()->FocusLocation(/*is_user_initiated=*/true,
                                /*clear_focus_if_failed=*/false);

  // Verify that the popup state is initially kFull.
  ASSERT_EQ(OmniboxPopupState::kFull,
            controller()->popup_state_manager()->popup_state());

  omnibox_view()->SetUserText(u"test query");

  auto dummy_widget = DeactivatePopupWidget();

  // Wait for the deferred deactivation task to run and verify that the popup
  // state transitions to kNone.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return controller()->popup_state_manager()->popup_state() !=
           OmniboxPopupState::kNone;
  }));
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUIFullV2Test,
                       DeactivationWithClearedTextClearsPopupState) {
  // Focus the location bar to ensure the Omnibox has active focus and the popup
  // is open.
  location_bar()->FocusLocation(/*is_user_initiated=*/true,
                                /*clear_focus_if_failed=*/false);

  // Verify that the popup state is initially kFull.
  ASSERT_EQ(OmniboxPopupState::kFull,
            controller()->popup_state_manager()->popup_state());

  omnibox_view()->SetUserText(u"test query");
  omnibox_view()->SetUserText(u"");

  auto dummy_widget = DeactivatePopupWidget();

  // Wait for the deferred deactivation task to run and verify that the popup
  // state transitions to kNone.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return controller()->popup_state_manager()->popup_state() ==
           OmniboxPopupState::kNone;
  }));
}

class OmniboxPopupDimensionsTest : public OmniboxPopupViewWebUITest,
                                   public testing::WithParamInterface<bool> {
 public:
  bool has_results() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, OmniboxPopupDimensionsTest, testing::Bool());

// Tests that the popup has the correct dimensions and alignment, both with
// and without results (for the base WebUI popup).
// Note: This expects the height to include alignment and shadow insets.
IN_PROC_BROWSER_TEST_P(OmniboxPopupDimensionsTest, DimensionsAndAnchoring) {
  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  ASSERT_TRUE(popup_view);

  if (has_results()) {
    // Creates a popup with results.
    CreatePopupForTestQuery();
  } else {
    popup_view->presenter()->Show();
  }

  auto* presenter = popup_view->presenter();
  ASSERT_TRUE(presenter);

  views::Widget* widget = presenter->get_widget_for_testing();
  ASSERT_TRUE(widget);

  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Rect location_bar_bounds = location_bar()->GetBoundsInScreen();

  gfx::Rect expected_bounds = location_bar_bounds;
  expected_bounds.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  expected_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());

  // Width and bounds should match exactly.
  EXPECT_EQ(widget_bounds.width(), expected_bounds.width());
  EXPECT_EQ(widget_bounds.x(), expected_bounds.x());
  EXPECT_EQ(widget_bounds.y(), expected_bounds.y());
  int min_expected_height =
      location_bar_bounds.height() +
      RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().height() +
      RoundedOmniboxResultsFrame::GetShadowInsets().height();
  EXPECT_GE(widget_bounds.height(), min_expected_height);

  popup_view->presenter()->Hide();
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_PopupResizeWindow DISABLED_PopupResizeWindow
#else
#define MAYBE_PopupResizeWindow PopupResizeWindow
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUITest, MAYBE_PopupResizeWindow) {
  CreatePopupForTestQuery();

  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  ASSERT_TRUE(popup_view);

  views::Widget* widget = popup_view->presenter()->get_widget_for_testing();
  ASSERT_TRUE(widget);

  // Resize window smaller.
  gfx::Rect current_browser_bounds = browser()->GetWindow()->GetBounds();
  gfx::Rect new_browser_bounds = current_browser_bounds;
  new_browser_bounds.set_width(current_browser_bounds.width() - 200);
  browser()->GetWindow()->SetBounds(new_browser_bounds);

  // Give it a moment to layout.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetWindow()->GetBounds().width() ==
           new_browser_bounds.width();
  }));

  gfx::Rect new_widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Rect location_bar_bounds = location_bar()->GetBoundsInScreen();
  gfx::Rect expected_bounds = location_bar_bounds;
  expected_bounds.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  expected_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());

  EXPECT_EQ(new_widget_bounds.width(), expected_bounds.width());
  EXPECT_EQ(new_widget_bounds.x(), expected_bounds.x());

  // Resize window larger.
  new_browser_bounds.set_width(current_browser_bounds.width() + 200);
  browser()->GetWindow()->SetBounds(new_browser_bounds);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetWindow()->GetBounds().width() ==
           current_browser_bounds.width() + 200;
  }));

  new_widget_bounds = widget->GetWindowBoundsInScreen();
  location_bar_bounds = location_bar()->GetBoundsInScreen();
  expected_bounds = location_bar_bounds;
  expected_bounds.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  expected_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());

  EXPECT_EQ(new_widget_bounds.width(), expected_bounds.width());
  EXPECT_EQ(new_widget_bounds.x(), expected_bounds.x());
}

namespace {

class TestPermissionPromptDelegate
    : public permissions::PermissionPrompt::Delegate {
 public:
  explicit TestPermissionPromptDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    request_list_.push_back(
        std::make_unique<permissions::MockPermissionRequest>(
            permissions::RequestType::kMicStream,
            permissions::PermissionRequestGestureType::GESTURE));
  }

  const std::vector<std::unique_ptr<permissions::PermissionRequest>>& Requests()
      override {
    return request_list_;
  }
  GURL GetRequestingOrigin() const override {
    return GURL(permissions::MockPermissionRequest::kDefaultOrigin);
  }
  GURL GetEmbeddingOrigin() const override {
    return GURL(permissions::MockPermissionRequest::kDefaultOrigin);
  }
  void Accept(const PromptOptions& prompt_options) override {}
  void AcceptThisTime(const PromptOptions& prompt_options) override {}
  void Deny(const PromptOptions& prompt_options) override {}
  void Dismiss(const PromptOptions& prompt_options) override {}
  void Ignore(const PromptOptions& prompt_options) override {}
  void SwitchToLoudPrompt() override {}
  GeolocationAccuracy GetInitialGeolocationAccuracySelection() const override {
    return GeolocationAccuracy::kPrecise;
  }
  std::optional<permissions::GeolocationPromptType> GetGeolocationPromptType()
      const override {
    return std::nullopt;
  }
  void FinalizeCurrentRequests() override {}
  void OpenHelpCenterLink(const ui::Event&) override {}
  void PreIgnoreQuietPrompt() override {}
  void SetManageClicked() override {}
  void SetLearnMoreClicked() override {}
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override {}
  std::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override {
    return std::nullopt;
  }
  bool ShouldCurrentRequestUseQuietUI() const override { return false; }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return false;
  }
  bool WasCurrentRequestAlreadyDisplayed() override { return false; }
  void SetDismissOnTabClose() override {}
  void SetPromptShown() override {}
  void SetDecisionTime() override {}
  bool RecreateView() override { return false; }
  const permissions::PermissionPrompt* GetCurrentPrompt() const override {
    return nullptr;
  }
  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  content::WebContents* GetAssociatedWebContents() override {
    return web_contents_;
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> request_list_;
  base::WeakPtrFactory<TestPermissionPromptDelegate> weak_factory_{this};
};

}  // namespace

// Verifies that when the regular WebUI Omnibox popup is open, creating a
// permission prompt via `PermissionPromptFactory` notifies the presenter
// synchronously and prevents the popup from closing.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUITest,
                       PermissionPromptCreationLocksRegularWebUIPresenter) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));

  CreatePopupForTestQuery();

  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  ASSERT_TRUE(popup_view);
  auto* presenter = popup_view->presenter();
  ASSERT_TRUE(presenter);
  EXPECT_TRUE(presenter->IsShown());

  // Initially, before `PermissionPromptFactory` runs, presenter is NOT locked.
  // (Verifies `PermissionRequestManager` did NOT set it).
  EXPECT_FALSE(presenter->IsPermissionPromptPreventingClose());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  TestPermissionPromptDelegate test_delegate(web_contents);

  // Directly call `PermissionPromptFactory::CreatePermissionPrompt`
  // synchronously.
  CreatePermissionPrompt(web_contents, &test_delegate);

  // Regular WebUI popup presenter MUST be locked synchronously by
  // `PermissionPromptFactory`.
  EXPECT_TRUE(presenter->IsPermissionPromptPreventingClose());
}

#if BUILDFLAG(IS_MAC)
class OmniboxPopupViewWebUIFrameCacheTest
    : public OmniboxPopupViewWebUITest,
      public testing::WithParamInterface<bool> {
  // TODO(crbug.com/538294830): Use base/test/with_feature_override.h instead.
 public:
  OmniboxPopupViewWebUIFrameCacheTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {omnibox::kOmniboxWebUIPopupMarkAsHidden,
           ::features::kHideDelegatedFrameHostMac},
          {omnibox::kOmniboxWebUIDetachWebContentsOnHide});
    } else {
      feature_list_.InitWithFeatures(
          {omnibox::kOmniboxWebUIPopupMarkAsHidden},
          {omnibox::kOmniboxWebUIDetachWebContentsOnHide,
           ::features::kHideDelegatedFrameHostMac});
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxPopupViewWebUIFrameCacheTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(OmniboxPopupViewWebUIFrameCacheTest, FrameCacheUsage) {
  // During browser startup, other WebUIs (or the initial NTP view) might have
  // generated their own unlocked frames that are cached in memory. Purging
  // ensures the baseline is 0.
  content::PurgeUnlockedCompositorFrames();
  EXPECT_EQ(0u, content::GetUnlockedCompositorFrameCount());

  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupView());
  popup_view->presenter()->Show();
  popup_view->UpdatePopupAppearance();

  popup_view->presenter()->Hide();

  // Verify that the frame is correctly unlocked when the feature fix is
  // enabled.
  if (GetParam()) {
    EXPECT_EQ(1u, content::GetUnlockedCompositorFrameCount());
  } else {
    EXPECT_EQ(0u, content::GetUnlockedCompositorFrameCount());
  }
}
#endif

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
