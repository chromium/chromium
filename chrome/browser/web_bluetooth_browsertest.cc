// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains browsertests for Web Bluetooth that depend on behavior
// defined in chrome/, not just in content/.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"

using device::MockBluetoothAdapter;
using testing::Return;

typedef testing::NiceMock<MockBluetoothAdapter> NiceMockBluetoothAdapter;

namespace {

class WebBluetoothTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(juncai): Remove this switch once Web Bluetooth is supported on Linux
    // and Windows.
    // https://crbug.com/570344
    // https://crbug.com/507419
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    // Navigate to a secure context.
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("localhost", "/simple_page.html"));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_THAT(
        web_contents_->GetMainFrame()->GetLastCommittedOrigin().Serialize(),
        testing::StartsWith("http://localhost:"));
  }

  content::WebContents* web_contents_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, DISABLED_WebBluetoothAfterCrash) {
  // Make sure we can use Web Bluetooth after the tab crashes.
  // Set up adapter with one device.
  scoped_refptr<NiceMockBluetoothAdapter> adapter(
      new NiceMockBluetoothAdapter());
  ON_CALL(*adapter, IsPresent()).WillByDefault(Return(false));

  auto bt_global_values =
      device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
  bt_global_values->SetLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{services: [0x180d]}]})"
      "  .catch(e => domAutomationController.send(e.toString()));",
      &result));
  EXPECT_EQ("NotFoundError: Bluetooth adapter not available.", result);

  // Crash the renderer process.
  content::RenderProcessHost* process =
      web_contents_->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // Reload tab.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Use Web Bluetooth again.
  std::string result_after_crash;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{services: [0x180d]}]})"
      "  .catch(e => domAutomationController.send(e.toString()));",
      &result_after_crash));
  EXPECT_EQ("NotFoundError: Bluetooth adapter not available.",
            result_after_crash);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, KillSwitchShouldBlock) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(Return(true));
  auto bt_global_values =
      device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
  bt_global_values->SetLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // Turn on the global kill switch.
  std::map<std::string, std::string> params;
  params["Bluetooth"] =
      PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  variations::AssociateVariationParams(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup",
      params);
  base::FieldTrialList::CreateFieldTrial(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup");

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection,
              testing::MatchesRegex("NotFoundError: .*globally disabled.*"));
}

// Tests that using Finch field trial parameters for blocklist additions has
// the effect of rejecting requestDevice calls.
IN_PROC_BROWSER_TEST_F(WebBluetoothTest, BlocklistShouldBlock) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(Return(true));
  auto bt_global_values =
      device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
  bt_global_values->SetLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  if (base::FieldTrialList::TrialExists("WebBluetoothBlocklist")) {
    LOG(INFO) << "WebBluetoothBlocklist field trial already configured.";
    ASSERT_NE(variations::GetVariationParamValue("WebBluetoothBlocklist",
                                                 "blocklist_additions")
                  .find("ed5f25a4"),
              std::string::npos)
        << "ERROR: WebBluetoothBlocklist field trial being tested in\n"
           "testing/variations/fieldtrial_testing_config_*.json must\n"
           "include this test's random UUID 'ed5f25a4' in\n"
           "blocklist_additions.\n";
  } else {
    LOG(INFO) << "Creating WebBluetoothBlocklist field trial for test.";
    // Create a field trial with test parameter.
    std::map<std::string, std::string> params;
    params["blocklist_additions"] = "ed5f25a4:e";
    variations::AssociateVariationParams("WebBluetoothBlocklist", "TestGroup",
                                         params);
    base::FieldTrialList::CreateFieldTrial("WebBluetoothBlocklist",
                                           "TestGroup");
  }

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{services: [0xed5f25a4]}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection,
              testing::MatchesRegex("SecurityError: .*blocklisted UUID.*"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, NavigateWithChooserCrossOrigin) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(Return(true));
  auto bt_global_values =
      device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
  bt_global_values->SetLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(
      web_contents, 1 /* number_of_navigations */,
      content::MessageLoopRunner::QuitMode::DEFERRED);

  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]});"
      "document.location.href = \"https://google.com\";"));

  observer.Wait();
  EXPECT_EQ(0u, browser()->GetBubbleManager()->GetBubbleCountForTesting());
  EXPECT_EQ(GURL("https://google.com"), web_contents->GetLastCommittedURL());
}

}  // namespace
