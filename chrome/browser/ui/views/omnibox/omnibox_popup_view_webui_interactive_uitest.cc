// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

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
    return browser_view->toolbar()->location_bar();
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

  // Wait until page remote is bound and ready to receive calls.
  void WaitForHandler();

 private:
  OmniboxTriggeredFeatureService triggered_feature_service_;
  base::test::ScopedFeatureList feature_list_;
};

OmniboxPopupViewWebUITest::OmniboxPopupViewWebUITest() = default;
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
            ChromeAutocompleteSchemeClassifier(browser()->profile()));
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
      ThemeServiceFactory::GetForProfile(browser()->profile());
  if (!theme_service->UsingDefaultTheme()) {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseDefaultTheme();
  }
  ASSERT_TRUE(theme_service->UsingDefaultTheme());
#endif  // BUILDFLAG(IS_LINUX)
}

void OmniboxPopupViewWebUITest::SetUp() {
  feature_list_.InitAndEnableFeature(omnibox::kWebUIOmniboxPopup);
  InProcessBrowserTest::SetUp();
}

void OmniboxPopupViewWebUITest::WaitForHandler() {
  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupViewForTesting());
  auto* omnibox_popup_webui_content = popup_view->presenter_->GetWebUIContent();

  auto* web_contents = omnibox_popup_webui_content->GetWebContents();
  content::WaitForLoadStop(web_contents);

  WebuiOmniboxHandler* handler =
      static_cast<OmniboxPopupUI*>(web_contents->GetWebUI()->GetController())
          ->omnibox_handler();
  base::test::TestFuture<void> future;
  handler->set_page_is_bound_callback_for_testing(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(handler->IsRemoteBound());
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
  WaitForHandler();
  auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
      location_bar()->GetOmniboxPopupViewForTesting());
  popup_view->presenter_->Show();
  popup_view->UpdatePopupAppearance();
  OmniboxPopupSelection selection(OmniboxPopupSelection::kNoMatch);
  popup_view->ProvideButtonFocusHint(0);
  popup_view->presenter_->Hide();
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
