// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Runs the WebUI resources tests.
 */

namespace ash {

namespace {

class DiagnosticsAppBrowserTest : public WebUIMochaBrowserTest {
 public:
  DiagnosticsAppBrowserTest() {
    set_test_loader_host(ash::kChromeUIDiagnosticsAppHost);
  }

 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath =
        base::StringPrintf("chromeos/diagnostics/%s", testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, App) {
  RunTestAtPath("diagnostics_app_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, BatteryStatusCard) {
  RunTestAtPath("battery_status_card_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, CellularInfo) {
  RunTestAtPath("cellular_info_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, ConnectivityCard) {
  RunTestAtPath("connectivity_card_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, CpuCard) {
  RunTestAtPath("cpu_card_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, DataPoint) {
  RunTestAtPath("data_point_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, DiagnosticsNetworkIcon) {
  RunTestAtPath("diagnostics_network_icon_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, DiagnosticsStickyBanner) {
  RunTestAtPath("diagnostics_sticky_banner_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, DiagnosticsUtils) {
  RunTestAtPath("diagnostics_utils_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, DrawingProvider) {
  RunTestAtPath("drawing_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, DrawingProviderUtils) {
  RunTestAtPath("drawing_provider_utils_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, EthernetInfo) {
  RunTestAtPath("ethernet_info_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, FakeMojoInterface) {
  RunTestAtPath("mojo_interface_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, FakeNetworkHealthProvider) {
  RunTestAtPath("fake_network_health_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, FakeSystemDataProvider) {
  RunTestAtPath("fake_system_data_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, FakeSystemRoutineController) {
  RunTestAtPath("fake_system_routine_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, FrequencyChannelUtils) {
  RunTestAtPath("frequency_channel_utils_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, IpConfigInfoDrawer) {
  RunTestAtPath("ip_config_info_drawer_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, MemoryCard) {
  RunTestAtPath("memory_card_test.js");
}

// TODO(crbug.com/339850572): Flaky
IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, DISABLED_NetworkCard) {
  RunTestAtPath("network_card_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, NetworkInfo) {
  RunTestAtPath("network_info_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, NetworkList) {
  RunTestAtPath("network_list_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, NetworkTroubleshooting) {
  RunTestAtPath("network_troubleshooting_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, OverviewCard) {
  RunTestAtPath("overview_card_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, PercentBarChart) {
  RunTestAtPath("percent_bar_chart_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, RealtimeCpuChart) {
  RunTestAtPath("realtime_cpu_chart_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, RoutineGroup) {
  RunTestAtPath("routine_group_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, RoutineListExecutor) {
  RunTestAtPath("routine_list_executor_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, RoutineResultEntry) {
  RunTestAtPath("routine_result_entry_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, RoutineResultList) {
  RunTestAtPath("routine_result_list_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, RoutineSection) {
  RunTestAtPath("routine_section_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, SystemPage) {
  RunTestAtPath("system_page_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, TextBadge) {
  RunTestAtPath("text_badge_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppBrowserTest, WifiInfo) {
  RunTestAtPath("wifi_info_test.js");
}

class DiagnosticsAppWithInputBrowserTest : public DiagnosticsAppBrowserTest {
 public:
  DiagnosticsAppWithInputBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEnableTouchpadsInDiagnosticsApp,
                              features::kEnableTouchscreensInDiagnosticsApp},
        /*disabled_features=*/{});
    set_test_loader_host(ash::kChromeUIDiagnosticsAppHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DiagnosticsAppWithInputBrowserTest, AppForInputHiding) {
  RunTestAtPath("diagnostics_app_input_hiding_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppWithInputBrowserTest, InputCard) {
  RunTestAtPath("input_card_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppWithInputBrowserTest, InputList) {
  RunTestAtPath("input_list_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppWithInputBrowserTest, KeyboardTester) {
  RunTestAtPath("keyboard_tester_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppWithInputBrowserTest, TouchscreenTester) {
  RunTestAtPath("touchscreen_tester_test.js");
}

IN_PROC_BROWSER_TEST_F(DiagnosticsAppWithInputBrowserTest, TouchpadTester) {
  RunTestAtPath("touchpad_tester_test.js");
}

}  // namespace

}  // namespace ash
