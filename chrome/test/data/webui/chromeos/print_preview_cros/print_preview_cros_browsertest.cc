// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://os-print.
 */
namespace ash {

namespace {

class PrintPreviewCrosBrowserTest : public WebUIMochaBrowserTest {
 public:
  PrintPreviewCrosBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPrintPreviewCrosApp},
        /*disabled_features=*/{});
    set_test_loader_host(::ash::kChromeUIPrintPreviewCrosHost);
  }

 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath = base::StringPrintf("chromeos/print_preview_cros/%s",
                                       testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, CapabilitiesManagerTest) {
  RunTestAtPath("capabilities_manager_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest,
                       DestinationDropdownControllerTest) {
  RunTestAtPath("destination_dropdown_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, DestinationDropdownTest) {
  RunTestAtPath("destination_dropdown_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, DestinationManagerTest) {
  RunTestAtPath("destination_manager_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest,
                       DestinationRowControllerTest) {
  RunTestAtPath("destination_row_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, DestinationRowTest) {
  RunTestAtPath("destination_row_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest,
                       DestinationSelectControllerTest) {
  RunTestAtPath("destination_select_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, DestinationSelectTest) {
  RunTestAtPath("destination_select_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, EventUtilsTest) {
  RunTestAtPath("event_utils_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest,
                       FakeDestinationProviderTest) {
  RunTestAtPath("fake_destination_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest,
                       FakePrintPreviewPageHandlerTest) {
  RunTestAtPath("fake_print_preview_page_handler_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest,
                       PrintPreviewCrosAppControllerTest) {
  RunTestAtPath("print_preview_cros_app_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, PrintPreviewCrosAppTest) {
  RunTestAtPath("print_preview_cros_app_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, PreviewTicketManagerTest) {
  RunTestAtPath("preview_ticket_manager_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, PrintTicketManagerTest) {
  RunTestAtPath("print_ticket_manager_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest,
                       SummaryPanelControllerTest) {
  RunTestAtPath("summary_panel_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, SummaryPanelTest) {
  RunTestAtPath("summary_panel_test.js");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, ValidationUtilsTest) {
  RunTestAtPath("validation_utils_test.js");
}

}  // namespace

}  // namespace ash
