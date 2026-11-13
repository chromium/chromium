// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chrome/test/base/web_ui_test_data_source.h"
#include "components/history_clusters/core/features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "ui/webui/buildflags.h"
#include "ui/webui/mojo_web_ui_controller.h"

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "base/values.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"

namespace {

class TestMostVisitedWebUIController : public ui::MojoWebUIController {
 public:
  explicit TestMostVisitedWebUIController(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui,
                                /*enable_chrome_send=*/false,
                                /*enable_chrome_histograms=*/true) {
    content::WebUIDataSource* source =
        webui::CreateAndAddWebUITestDataSource(Profile::FromWebUI(web_ui));
    source->UseStringsJs();
    source->AddLocalizedStrings(MostVisitedHandler::GetWebUIDataSourceDict());
  }

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(TestMostVisitedWebUIController)

}  // namespace
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

typedef WebUIMochaFocusTest CrComponentsFocusTest;

#if BUILDFLAG(ENABLE_WEBUI_NTP)
IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, MostVisited) {
  content::ScopedWebUIConfigRegistration config(
      std::make_unique<
          content::DefaultWebUIConfig<TestMostVisitedWebUIController>>(
          content::kChromeUIScheme, chrome::kChromeUIWebUITestHost));
  RunTest("cr_components/most_visited_focus_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, CrShortcutInput) {
  set_test_loader_host(chrome::kChromeUIExtensionsHost);
  RunTest("cr_components/cr_shortcut_input/cr_shortcut_input_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, ComposeboxAimButton) {
  RunTest("cr_components/composebox/composebox_aim_button_test.js",
          "runMochaSuite('ComposeboxAimButtonFocusTest')");
}

class CrComponentsHistoryClustersFocusTest : public WebUIMochaFocusTest {
 protected:
  CrComponentsHistoryClustersFocusTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history_clusters::internal::kJourneysImages);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersFocusTest, All) {
  RunTest("cr_components/history_clusters/history_clusters_test.js",
          "runMochaSuite('HistoryClustersFocusTest')");
}
