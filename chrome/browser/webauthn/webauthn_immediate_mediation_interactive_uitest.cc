// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/combined_selector_sheet_view.h"
#include "chrome/browser/webauthn/enclave_authenticator_browsertest_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/window/dialog_client_view.h"

// These tests are disabled under MSAN. The enclave subprocess is written in
// Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace {

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

const char kHostname[] = "www.example.com";
const char kPagePath[] = "/webauthn/get-immediate.html";

const DeepQuery kGetImmediateButton{"#get-immediate-button"};
const DeepQuery kGetImmediateUvRequiredButton{
    "#get-immediate-uv-required-button"};
const DeepQuery kGetImmediateUvDiscouragedButton{
    "#get-immediate-uv-discouraged-button"};
const DeepQuery kSuccess{"#success-message"};
const DeepQuery kError{"#error-message"};
const DeepQuery kMessage{"#message-container"};

}  // namespace

using Fixture = InteractiveBrowserTestT<EnclaveAuthenticatorTestBase>;
class WebAuthnImmediateMediationTest : public Fixture {
 public:
  WebAuthnImmediateMediationTest() {
    feature_list_.InitWithFeatures(
        {device::kWebAuthnImmediateGet},
        {device::kWebAuthnImmediateRequestRateLimit});
  }

  ~WebAuthnImmediateMediationTest() override = default;

 protected:
  GURL GetHttpsURL(const std::string& hostname = kHostname,
                   const std::string& relative_url = kPagePath) {
    return https_server_.GetURL(hostname, relative_url);
  }

  auto WaitForElementWithText(const ui::ElementIdentifier& element_id,
                              WebContentsInteractionTestUtil::DeepQuery element,
                              const std::string& expected_substring) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementWithText);
    WebContentsInteractionTestUtil::StateChange state_change;
    state_change.event = kElementWithText;
    state_change.where = element;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.test_function =
        base::StringPrintf("(el) => { return el.innerText.includes('%s'); }",
                           expected_substring.c_str());
    return WaitForStateChange(element_id, state_change);
  }

  auto GetStepsUntilButtonClick(
      const DeepQuery& button_to_click = kGetImmediateButton) {
    return Steps(InstrumentTab(kTabId),
                 NavigateWebContents(kTabId, GetHttpsURL()),
                 WaitForWebContentsReady(kTabId, GetHttpsURL()),
                 ClickElement(kTabId, button_to_click));
  }

  auto GetNotAllowedSteps() {
    return Steps(GetStepsUntilButtonClick(),
                 WaitForElementVisible(kTabId, kError),
                 WaitForElementWithText(kTabId, kMessage, "NotAllowedError"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateMediationTest,
                       ImmediateMediationNotAllowedNoCredentials) {
  RunTestSequence(GetNotAllowedSteps());
}

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateMediationTest,
                       ImmediateMediationNotAllowedIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::BrowserActivationWaiter(incognito_browser).WaitForActivation();
  RunTestSequence(InContext(incognito_browser->window()->GetElementContext(),
                            GetNotAllowedSteps()));
}

class WebAuthnImmediateMediationWithBootstrappedEnclaveTest
    : public WebAuthnImmediateMediationTest {
 protected:
  void SetUpOnMainThread() override {
    WebAuthnImmediateMediationTest::SetUpOnMainThread();
    SimulateSuccessfulGpmPinCreation("123456");
  }
};

// TODO(crbug.com/422074323): Re-enable this test in ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SinglePasskeyDiscouragedUv DISABLED_SinglePasskeyDiscouragedUv
#else
#define MAYBE_SinglePasskeyDiscouragedUv SinglePasskeyDiscouragedUv
#endif
IN_PROC_BROWSER_TEST_F(WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
                       MAYBE_SinglePasskeyDiscouragedUv) {
  AddTestPasskeyToModel();
  RunTestSequence(
      GetStepsUntilButtonClick(kGetImmediateButton),
      WaitForShow(CombinedSelectorSheetView::kCombinedSelectorSheetViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForElementVisible(kTabId, kSuccess));
}

// TODO(crbug.com/422074323): Re-enable this test in ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SinglePasskeyUvRequired DISABLED_SinglePasskeyUvRequired
#else
#define MAYBE_SinglePasskeyUvRequired SinglePasskeyUvRequired
#endif
IN_PROC_BROWSER_TEST_F(WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
                       MAYBE_SinglePasskeyUvRequired) {
  AddTestPasskeyToModel();
  RunTestSequence(
      GetStepsUntilButtonClick(kGetImmediateUvRequiredButton),
      WaitForShow(CombinedSelectorSheetView::kCombinedSelectorSheetViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForShow(AuthenticatorGpmPinSheetView::kGpmPinSheetViewId));
  // TODO(crbug.com/422074323): Add more steps to complete the UV flow.
}
#endif  // !defined(MEMORY_SANITIZER)
