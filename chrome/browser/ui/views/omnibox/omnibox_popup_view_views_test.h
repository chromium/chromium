// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_VIEWS_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_VIEWS_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/views/widget/widget.h"

// Base class for omnibox browser and ui tests.
class OmniboxPopupViewViewsTest : public InProcessBrowserTest {
 public:
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

  OmniboxPopupViewViewsTest() {}

  OmniboxPopupViewViewsTest(const OmniboxPopupViewViewsTest&) = delete;
  OmniboxPopupViewViewsTest& operator=(const OmniboxPopupViewViewsTest&) =
      delete;

  views::Widget* CreatePopupForTestQuery();
  views::Widget* GetPopupWidget() { return popup_view()->GetWidget(); }
  OmniboxResultView* GetResultViewAt(int index) {
    return popup_view()->result_view_at(index);
  }

  LocationBarView* location_bar() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar();
  }
  OmniboxViewViews* omnibox_view() { return location_bar()->omnibox_view(); }
  OmniboxController* controller() { return omnibox_view()->controller(); }
  OmniboxEditModel* edit_model() { return omnibox_view()->model(); }
  OmniboxPopupViewViews* popup_view() {
    return static_cast<OmniboxPopupViewViews*>(edit_model()->get_popup_view());
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

  void SetIsGrayscale(bool is_grayscale) {
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetIsGrayscale(is_grayscale);
  }

  void SetUseDeviceTheme(bool use_device_theme) {
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->UseDeviceTheme(use_device_theme);
  }

  // Some tests relies on the light/dark variants of the result background to be
  // different. But when using the system theme on Linux, these colors will be
  // the same. Ensure we're not using the system theme, which may be
  // conditionally enabled depending on the environment.
  void UseDefaultTheme();

  OmniboxTriggeredFeatureService* triggered_feature_service() {
    return &triggered_feature_service_;
  }

 private:
  OmniboxTriggeredFeatureService triggered_feature_service_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_VIEWS_TEST_H_
