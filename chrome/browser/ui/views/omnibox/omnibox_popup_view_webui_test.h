// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_TEST_H_

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "ui/views/widget/widget.h"

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
  OmniboxController* controller() { return omnibox_view()->controller(); }
  OmniboxEditModel* edit_model() { return omnibox_view()->model(); }
  OmniboxPopupViewWebUI* popup_view() {
    return static_cast<OmniboxPopupViewWebUI*>(edit_model()->get_popup_view());
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

  void SetUseDarkColor(bool use_dark) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    browser_view->GetNativeTheme()->set_use_dark_colors(use_dark);
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

  // Wait until WebUI page is loaded and handler is ready.
  void WaitForHandler();

 private:
  // Block until handler is ready.
  void WaitInternal(OmniboxPopupPresenter* presenter,
                    base::RepeatingClosure* closure);

  OmniboxTriggeredFeatureService triggered_feature_service_;
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<OmniboxPopupViewWebUITest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_TEST_H_
