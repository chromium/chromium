// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64url.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/combined_selector_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/combined_selector_views.h"
#include "chrome/browser/webauthn/enclave_authenticator_browsertest_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view_utils.h"
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
const char kUsername[] = "username";
const char kPassword[] = "password";
const char kUser1[] = "user1";
const char kUser2[] = "user2";

const DeepQuery kGetImmediateButton{"#get-immediate-button"};
const DeepQuery kGetImmediateUvRequiredButton{
    "#get-immediate-uv-required-button"};
const DeepQuery kGetImmediateUvDiscouragedButton{
    "#get-immediate-uv-discouraged-button"};
const DeepQuery kSuccess{"#success-message"};
const DeepQuery kError{"#error-message"};
const DeepQuery kMessage{"#message-container"};

}  // namespace

using Fixture = InteractiveBrowserTestMixin<EnclaveAuthenticatorTestBase>;
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
  RunTestSequenceInContext(
      BrowserElements::From(incognito_browser)->GetContext(),
      GetNotAllowedSteps());
}

class WebAuthnImmediateMediationWithBootstrappedEnclaveTest
    : public WebAuthnImmediateMediationTest {
 protected:
  void SetUpOnMainThread() override {
    WebAuthnImmediateMediationTest::SetUpOnMainThread();
    SimulateSuccessfulGpmPinCreation("123456");
  }

  void AddPassword(const std::string& username, const std::string& password) {
    password_manager::PasswordForm form;
    form.username_value = base::ASCIIToUTF16(username);
    form.password_value = base::ASCIIToUTF16(password);
    form.signon_realm = GetHttpsURL().DeprecatedGetOriginAsURL().spec();
    form.url = GetHttpsURL().DeprecatedGetOriginAsURL();
    form.match_type = password_manager::PasswordForm::MatchType::kExact;

    scoped_refptr<password_manager::PasswordStoreInterface> password_store =
        ProfilePasswordStoreFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    password_store->AddLogin(form);
  }

  void AddPasskey(const std::string& username,
                  base::span<const uint8_t> credential_id) {
    sync_pb::WebauthnCredentialSpecifics passkey;
    CHECK(passkey.ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
    passkey.set_user_name(username);
    passkey.set_user_display_name(username);
    passkey.set_user_id(base::RandBytesAsString(16));
    passkey.set_credential_id(
        std::string(credential_id.begin(), credential_id.end()));
    passkey.set_sync_id(base::RandBytesAsString(16));
    passkey_model().AddNewPasskeyForTesting(passkey);
  }

  auto SelectPasskeyInCombinedSelectorSheet(const std::string& username) {
    return Steps(
        NameDescendantView(
            CombinedSelectorSheetView::kCombinedSelectorSheetViewId,
            "SelectedPasskey",
            base::BindRepeating(
                [](const std::string& username, const views::View* view) {
                  if (!views::IsViewClass<views::RadioButton>(view)) {
                    return false;
                  }
                  const views::View* parent = view->parent();
                  if (!parent) {
                    return false;
                  }
                  // CombinedSelectorRowView sets its accessible name.
                  const std::u16string& name =
                      parent->GetViewAccessibility().GetCachedName();
                  return name.find(base::ASCIIToUTF16(username)) !=
                         std::string::npos;
                },
                username)),
        PressButton("SelectedPasskey"));
  }
};

// TODO(crbug.com/422074323): Re-enable this test suite in ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest \
  DISABLED_WebAuthnImmediateMediationWithBootstrappedEnclaveTest

class DISABLED_WebAuthnImmediateMediationWithBootstrappedEnclaveTest
    : public WebAuthnImmediateMediationWithBootstrappedEnclaveTest {};
#else
#define MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest \
  WebAuthnImmediateMediationWithBootstrappedEnclaveTest
#endif

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
    SinglePasskeyDiscouragedUv) {
  AddTestPasskeyToModel();
  RunTestSequence(
      GetStepsUntilButtonClick(kGetImmediateButton),
      WaitForShow(CombinedSelectorSheetView::kCombinedSelectorSheetViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForElementVisible(kTabId, kSuccess));
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
    SinglePasskeyUvRequired) {
  AddTestPasskeyToModel();
  RunTestSequence(
      GetStepsUntilButtonClick(kGetImmediateUvRequiredButton),
      WaitForShow(CombinedSelectorSheetView::kCombinedSelectorSheetViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForShow(AuthenticatorGpmPinSheetView::kGpmPinSheetViewId));
  // TODO(crbug.com/422074323): Add more steps to complete the UV flow.
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
    ImmediateMediationPassword) {
  AddPassword(kUsername, kPassword);
  RunTestSequence(GetNotAllowedSteps());
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
    ImmediateMediationPasswordAndPasskey) {
  AddPassword(kUsername, kPassword);
  AddTestPasskeyToModel();
  RunTestSequence(
      GetStepsUntilButtonClick(kGetImmediateButton),
      WaitForShow(CombinedSelectorSheetView::kCombinedSelectorSheetViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForElementVisible(kTabId, kSuccess));
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
    ImmediateMediationTwoPasskeys) {
  // Add a first passkey.
  auto cred_id1 = base::RandBytesAsVector(16);
  AddPasskey(kUser1, cred_id1);

  // Add a second passkey with a different credential ID.
  auto cred_id2 = base::RandBytesAsVector(16);
  std::string cred_id2_base64;
  base::Base64UrlEncode(cred_id2, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &cred_id2_base64);
  AddPasskey(kUser2, cred_id2);

  ASSERT_EQ(passkey_model().GetAllPasskeys().size(), 2u);

  RunTestSequence(
      GetStepsUntilButtonClick(kGetImmediateButton),
      WaitForShow(CombinedSelectorSheetView::kCombinedSelectorSheetViewId),
      // Select the second passkey.
      SelectPasskeyInCombinedSelectorSheet(kUser2),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForElementVisible(kTabId, kSuccess),
      WaitForElementWithText(kTabId, kMessage, cred_id2_base64));
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebAuthnImmediateMediationWithBootstrappedEnclaveTest,
    ImmediateMediationPasswordAndPasskeySameName) {
  AddPassword(kUser1, kPassword);
  AddTestPasskeyToModel();
  RunTestSequence(
      GetStepsUntilButtonClick(kGetImmediateButton),
      WaitForShow(CombinedSelectorSheetView::kCombinedSelectorSheetViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForElementVisible(kTabId, kSuccess));
}

#endif  // !defined(MEMORY_SANITIZER)
