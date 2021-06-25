// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/webauthn/webauthn_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/authenticator_environment.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class WebAuthUITest : public InProcessBrowserTest {
 public:
  WebAuthUITest(const WebAuthUITest&) = delete;
  WebAuthUITest& operator=(const WebAuthUITest&) = delete;

 protected:
  WebAuthUITest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebAuthConditionalUI);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_.Start());
  }

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  AuthenticatorRequestDialogModel* dialog_model_;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When a conditional UI WebAuthn request is made, the browser should show an
// icon on the omnibar if the tab executing the request is focused.
IN_PROC_BROWSER_TEST_F(WebAuthUITest, ConditionalUI) {
  ui_test_utils::NavigateToURL(browser(),
                               GetHttpsURL("www.example.com", "/title1.html"));
  auto owned_virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  auto* virtual_device_factory = owned_virtual_device_factory.get();
  content::AuthenticatorEnvironment::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::move(owned_virtual_device_factory));
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.resident_key_support = true;
  virtual_device_factory->SetCtap2Config(std::move(config));
  virtual_device_factory->mutable_state()->InjectResidentKey(
      std::vector<uint8_t>{1, 2, 3, 4}, "www.example.com",
      std::vector<uint8_t>{6, 7, 8, 9}, /*user_name=*/base::nullopt,
      /*user_display_name=*/base::nullopt);
  virtual_device_factory->mutable_state()->fingerprints_enrolled = true;
  PageActionIconView* webauthn_icon =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kWebAuthn);

  // The icon should not be visible if there is no ongoing conditional UI
  // request.
  EXPECT_FALSE(webauthn_icon->GetActive());
  EXPECT_FALSE(webauthn_icon->GetVisible());
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([webauthn_icon, browser = browser()](
                                     device::VirtualFidoDevice* device) {
        // The icon should only appear during a conditional UI request for
        // the correct tab.
        EXPECT_TRUE(webauthn_icon->GetVisible());

        chrome::NewTab(browser);
        EXPECT_FALSE(webauthn_icon->GetVisible());

        chrome::CloseTab(browser);
        EXPECT_TRUE(webauthn_icon->GetVisible());
        return true;
      });

  constexpr char kGetAssertion[] =
      "navigator.credentials.get({"
      "  publicKey: {"
      "    challenge: new Uint8Array([1,2,3,4]),"
      "    timeout: 1000,"
      "  },"
      "  mediation: 'conditional'"
      "}).then(c => window.domAutomationController.send(c ? 'OK' : 'c null'),"
      "         e => window.domAutomationController.send(e.toString()));";
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(web_contents,
                                                     kGetAssertion, &result));
  EXPECT_EQ(result, "OK");
  EXPECT_FALSE(webauthn_icon->GetActive());
  EXPECT_FALSE(webauthn_icon->GetVisible());
}

}  // anonymous namespace
