// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

class NewTabPageBrowserTest : public WebUIMochaBrowserTest {
 protected:
  NewTabPageBrowserTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
  }
};

using NewTabPageTest = NewTabPageBrowserTest;

IN_PROC_BROWSER_TEST_F(NewTabPageTest, MetricsUtils) {
  RunTest("new_tab_page/metrics_utils_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, VoiceSearchOverlay) {
  RunTest("new_tab_page/voice_search_overlay_test.js", "mocha.run()");
}

// TODO(crbug.com/40933410):  Re-enable once no longer fails.
IN_PROC_BROWSER_TEST_F(NewTabPageTest, DISABLED_LensForm) {
  RunTest("new_tab_page/lens_form_test.js", "mocha.run()");
}

// TODO(crbug.com/40902230): Test is flaky across platforms.
IN_PROC_BROWSER_TEST_F(NewTabPageTest, DISABLED_LensUploadDialog) {
  RunTest("new_tab_page/lens_upload_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, Logo) {
  RunTest("new_tab_page/logo_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, DoodleShareDialog) {
  RunTest("new_tab_page/doodle_share_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, BackgroundManager) {
  RunTest("new_tab_page/background_manager_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, MiddleSlotPromo) {
  RunTest("new_tab_page/middle_slot_promo_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, MobilePromo) {
  RunTest("new_tab_page/mobile_promo_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, ImageProcessor) {
  RunTest("new_tab_page/image_processor_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageTest, Transparency) {
  RunTest("new_tab_page/transparency_test.js", "mocha.run()");
}

using NewTabPageModulesTest = NewTabPageBrowserTest;

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, ModuleWrapper) {
  RunTest("new_tab_page/modules/module_wrapper_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, ModulesV2) {
  RunTest("new_tab_page/modules/v2/modules_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, ModuleHeaderV2) {
  RunTest("new_tab_page/modules/v2/module_header_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, Modules) {
  RunTest("new_tab_page/modules/modules_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, ModuleDescriptor) {
  RunTest("new_tab_page/modules/module_descriptor_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, ModuleRegistry) {
  RunTest("new_tab_page/modules/module_registry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, ModuleHeader) {
  RunTest("new_tab_page/modules/module_header_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, InfoDialog) {
  RunTest("new_tab_page/modules/info_dialog_test.js", "mocha.run()");
}

#if !defined(OFFICIAL_BUILD)
// The dummy module is not available in official builds.
IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, DummyModule) {
  RunTest("new_tab_page/modules/v2/dummy/module_test.js", "mocha.run()");
}
#endif  // !defined(OFFICIAL_BUILD)

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, Calendar) {
  RunTest("new_tab_page/modules/v2/calendar/calendar_test.js",
          "runMochaSuite('NewTabPageModulesCalendarTest general')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, CalendarMetrics) {
  RunTest("new_tab_page/modules/v2/calendar/calendar_test.js",
          "runMochaSuite('NewTabPageModulesCalendarTest metrics')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, CalendarEvent) {
  RunTest("new_tab_page/modules/v2/calendar/calendar_event_test.js",
          "runMochaSuite('NewTabPageModulesCalendarEventTest general')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, CalendarEventMetrics) {
  RunTest("new_tab_page/modules/v2/calendar/calendar_event_test.js",
          "runMochaSuite('NewTabPageModulesCalendarEventTest metrics')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, GoogleCalendarModule) {
  RunTest("new_tab_page/modules/v2/calendar/google_calendar_module_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, OutlookCalendarModule) {
  RunTest("new_tab_page/modules/v2/calendar/outlook_calendar_module_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, DriveModule) {
  RunTest("new_tab_page/modules/drive/module_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesTest, FileSuggestionModule) {
  RunTest("new_tab_page/modules/v2/file_suggestion/module_test.js",
          "mocha.run()");
}

using NewTabPageAppTest = NewTabPageBrowserTest;

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Misc) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Misc')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, OgbThemingRemoveScrim) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest OgbThemingRemoveScrim')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, OgbScrim) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest OgbScrim')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Theming) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Theming')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Promo) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Promo')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Clicks) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Clicks')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Modules) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Modules')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, V2Modules) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest V2Modules')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, CounterfactualModules) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest CounterfactualModules')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, CustomizeChromeSidePanel) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest CustomizeChromeSidePanel')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, LensUploadDialog) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest LensUploadDialog')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, WallpaperSearch) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest WallpaperSearch')");
}

class NewTabPageModulesMostRelevantTabResumptionModuleTest
    : public NewTabPageBrowserTest {
 protected:
  NewTabPageModulesMostRelevantTabResumptionModuleTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::
                                  kNtpMostRelevantTabResumptionModule},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageModulesMostRelevantTabResumptionModuleTest,
                       Core) {
  RunTest("new_tab_page/modules/v2/most_relevant_tab_resumption/module_test.js",
          "runMochaSuite('NewTabPageModulesMostRelevantTabResumptionModuleTest "
          "Core')");
}
