// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "device/fido/public/features.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

constexpr char kHostname[] = "www.example.com";

// Records `blur` events on the window so the test can check whether showing
// the Ambient UI took focus away from the page.
constexpr char kInstallBlurListener[] = R"(
  () => {
    window.blurCount = 0;
    window.addEventListener('blur', () => { window.blurCount++; });
    return document.hasFocus();
  })";

// Starts an Ambient request. The promise is intentionally not returned because
// it doesn't settle until the user selects a credential.
constexpr char kStartAmbientRequest[] = R"(
  () => {
    navigator.credentials.get({
      uiMode: 'passive',
      mediation: 'conditional',
      publicKey: {
        challenge: new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]),
        timeout: 10000,
        userVerification: 'discouraged',
        allowCredentials: [],
      }});
  })";

class WebAuthnAmbientFocusTest : public InteractiveBrowserTest {
 public:
  WebAuthnAmbientFocusTest() {
    scoped_feature_list_.InitAndEnableFeature(device::kWebAuthnAmbientSignin);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    https_server_.SetCertHostnames({kHostname});
    ASSERT_TRUE(https_server_.Start());
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    // A platform authenticator with a discoverable credential for the site, so
    // that the Ambient UI has something to show.
    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    config.internal_uv_support = true;
    config.is_platform_authenticator = true;
    virtual_device_factory->SetTransport(
        device::FidoTransportProtocol::kInternal);
    virtual_device_factory->SetCtap2Config(std::move(config));
    virtual_device_factory->mutable_state()->InjectResidentKey(
        std::vector<uint8_t>{1, 2, 3, 4}, kHostname,
        std::vector<uint8_t>{5, 6, 7, 8}, "flandre", "Flandre Scarlet");
    virtual_device_factory->mutable_state()->fingerprints_enrolled = true;
    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));
  }

  void TearDownOnMainThread() override {
    auth_env_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  GURL GetHttpsURL() { return https_server_.GetURL(kHostname, "/title1.html"); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

// Showing the Ambient bubble automatically must not move focus away from the
// page. See https://crbug.com/587304987.
IN_PROC_BROWSER_TEST_F(WebAuthnAmbientFocusTest,
                       AutomaticBubbleDoesNotBlurPage) {
  RunTestSequence(
      InstrumentTab(kTabId), NavigateWebContents(kTabId, GetHttpsURL()),
      FocusWebContents(kTabId),
      CheckJsResult(kTabId, kInstallBlurListener, true),
      ExecuteJs(kTabId, kStartAmbientRequest),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      CheckJsResult(kTabId, "() => window.blurCount", 0),
      CheckJsResult(kTabId, "() => document.hasFocus()", true),
      CheckView(
          PermissionPromptBubbleBaseView::kMainViewId,
          [](views::View* bubble) { return bubble->GetWidget()->IsActive(); },
          false));
}

}  // namespace
