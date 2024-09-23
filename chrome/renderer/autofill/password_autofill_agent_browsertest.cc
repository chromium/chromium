// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/renderer/autofill/fake_mojo_password_manager_driver.h"
#include "chrome/renderer/autofill/fake_password_generation_driver.h"
#include "chrome/renderer/autofill/password_generation_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

namespace autofill {

namespace {

using autofill::FormRendererId;
using autofill::FormTracker;
using autofill::mojom::FocusedFieldType;
using autofill::mojom::SubmissionIndicatorEvent;
using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebString;
using testing::_;
using testing::AllOf;
using testing::AtMost;
using testing::Eq;
using testing::Field;
using testing::Truly;

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kPasswordName[] = "password";
const char kSearchField[] = "search";
const char kSocialMediaTextArea[] = "new_chirp";

const char kAliceUsername[] = "alice";
const char16_t kAliceUsername16[] = u"alice";
const char kAlicePassword[] = "password";
const char16_t kAlicePassword16[] = u"password";
const char kBobUsername[] = "bob";
const char16_t kBobUsername16[] = u"bob";
const char kBobPassword[] = "secret";
const char16_t kBobPassword16[] = u"secret";
const char16_t kCarolUsername16[] = u"Carol";
const char kCarolPassword[] = "test";
const char16_t kCarolPassword16[] = u"test";
const char16_t kCarolAlternateUsername16[] = u"RealCarolUsername";

const char kFormHTML[] =
    "<FORM id='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='random_field'/>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

#if BUILDFLAG(IS_ANDROID)
const char kFormWithUsernameFieldWebauthnHTML[] =
    "<FORM id='LoginTestForm' action='http://www.example.com'>"
    "  <INPUT type='text' id='username' autocomplete='webauthn'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kFormWithPasswordFieldWebauthnHTML[] =
    "<FORM id='LoginTestForm' action='http://www.example.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password' autocomplete='webauthn'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";
#endif  // BUILDFLAG(IS_ANDROID)

const char kSocialNetworkPostFormHTML[] =
    "<FORM id='SocialMediaPostForm' action='http://www.chirper.com'>"
    "  <TEXTAREA id='new_chirp'>"
    "  </TEXTAREA>"
    "  <INPUT type='submit' value='Chirp'/>"
    "</FORM>";

const char kSearchFieldHTML[] =
    "<FORM id='SearchFieldForm' action='http://www.gewgle.de'>"
    "  <INPUT type='search' id='search'/>"
    "  <INPUT type='submit' value='Chirp'/>"
    "</FORM>";

const char kWebAutnFieldHTML[] =
    "<FORM id='WebAuthnFieldForm' action='http://www.gewgle.de'>"
    "  <INPUT type='text' id='username' autocomplete='webauthn'/>"
    "  <INPUT type='password' id='password' autocomplete='webauthn'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kVisibleFormWithNoUsernameHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body>"
    "  <form name='LoginTestForm' action='http://www.bidule.com'>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kSingleUsernameFormHTML[] =
    "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username' autocomplete='username'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kSingleTextInputFormHTML[] =
    "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kEmptyFormHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body> <form> </form> </body>";

const char kFormWithoutPasswordsHTML[] =
    "<FORM>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='text' id='random_field'/>"
    "</FORM>";

const char kNonVisibleFormHTML[] =
    "<head> <style> form {visibility: hidden;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kNonDisplayedFormHTML[] =
    "<head> <style> form {display: none;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kSignupFormHTML[] =
    "<FORM id='LoginTestForm' name='LoginTestForm' "
    "    action='http://www.bidule.com'>"
    "  <INPUT type='text' id='random_info'/>"
    "  <INPUT type='password' id='new_password'/>"
    "  <INPUT type='password' id='confirm_password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kEmptyWebpage[] =
    "<html>"
    "   <head>"
    "   </head>"
    "   <body>"
    "   </body>"
    "</html>";

const char kRedirectionWebpage[] =
    "<html>"
    "   <head>"
    "       <meta http-equiv='Content-Type' content='text/html'>"
    "       <title>Redirection page</title>"
    "       <script></script>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "         function test(){}"
    "       </script>"
    "   </body>"
    "</html>";

const char kSimpleWebpage[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <form name='LoginTestForm'>"
    "           <input type='text' id='username'/>"
    "           <input type='checkbox' id='accept-tc'>"
    "           <input type='password' id='password'/>"
    "           <input type='checkbox' id='remember-me'>"
    "           <input type='submit' value='Login'/>"
    "       </form>"
    "   </body>"
    "</html>";

const char kWebpageWithDynamicContent[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "           function addParagraph() {"
    "             var p = document.createElement('p');"
    "             document.body.appendChild(p);"
    "            }"
    "           window.onload = addParagraph;"
    "       </script>"
    "   </body>"
    "</html>";

const char kJavaScriptClick[] =
    "var event = new MouseEvent('click', {"
    "   'view': window,"
    "   'bubbles': true,"
    "   'cancelable': true"
    "});"
    "var form = document.getElementById('myform1');"
    "form.dispatchEvent(event);"
    "console.log('clicked!');";

const char kPasswordChangeFormHTML[] =
    "<FORM name='ChangeWithUsernameForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='password' id='newpassword'/>"
    "  <INPUT type='password' id='confirmpassword'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kCreditCardFormHTML[] =
    "<FORM name='ChangeWithUsernameForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='creditcardowner'/>"
    "  <INPUT type='text' id='creditcardnumber'/>"
    "  <INPUT type='password' id='cvc'/>"
    "  <INPUT type='submit' value='Submit'/>"
    "</FORM>";

const char kNoFormHTML[] =
    "<script>"
    "  function on_keypress(event) {"
    "    if (event.which === 13) {"
    "      var field = document.getElementById('password');"
    "      field.parentElement.removeChild(field);"
    "    }"
    "  }"
    "</script>"
    "<INPUT type='text' id='username'/>"
    "<INPUT type='password' id='password' onkeypress='on_keypress(event)'/>";

const char kTwoNoUsernameFormsHTML[] =
    "<FORM name='form1' action='http://www.bidule.com'>"
    "  <INPUT type='password' id='password1' name='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>"
    "<FORM name='form2' action='http://www.bidule.com'>"
    "  <INPUT type='password' id='password2' name='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kDivWrappedFormHTML[] =
    "<DIV id='outer'>"
    "  <DIV id='inner'>"
    "    <FORM id='form' action='http://www.bidule.com'>"
    "      <INPUT type='text' id='username'/>"
    "      <INPUT type='password' id='password'/>"
    "    </FORM>"
    "  </DIV>"
    "</DIV>";

const char kJavaScriptRemoveForm[] =
    "var form = document.getElementById('LoginTestForm');"
    "form.parentNode.removeChild(form);";

const char kFormTagHostsShadowDomInputs[] =
    "<script>"
    "  function addShadowFields() {"
    "    const un_host = document.getElementById('un_host');"
    "    const un_shadow = un_host.attachShadow({ mode: 'open'});"
    "    const un = document.createElement('input');"
    "    un_shadow.appendChild(un);"
    "    const pw_host = document.getElementById('pw_host');"
    "    const pw_shadow = pw_host.attachShadow({ mode: 'open'});"
    "    const pw = document.createElement('input');"
    "    pw.type = 'password';"
    "    pw_shadow.appendChild(pw);"
    "}"
    "</script>"
    "<body onload='addShadowFields();'>"
    "<form method='POST' action='done.html' id='shadyform'>"
    "<div id='un_host'></div>"
    "<div id='pw_host'></div>"
    "<input type='submit' id='input_submit_button'>"
    "</form>"
    "</body>";

constexpr std::string_view kUnownedFieldsWithPasswordDisabled =
    "<input type='text' id='username'>"
    "<input type='password' disabled id='password'>";

// Sets the "readonly" attribute of `element` to the value given by `read_only`.
void SetElementReadOnly(WebInputElement& element, bool read_only) {
  element.SetAttribute(WebString::FromUTF8("readonly"),
                       read_only ? WebString::FromUTF8("true") : WebString());
}

bool FormHasFieldWithValue(const autofill::FormData& form,
                           const std::u16string& value) {
  for (const auto& field : form.fields()) {
    if (field.value() == value) {
      return true;
    }
    if (field.user_input() == value) {
      return true;
    }
  }
  return false;
}

enum PasswordFormSourceType {
  PasswordFormSubmitted,
  PasswordFormSameDocumentNavigation,
};

enum class FieldChangeSource {
  USER,
  AUTOFILL_SINGLE_FIELD,
  USER_AUTOFILL_SINGLE_FIELD,
  AUTOFILL_FORM,
  USER_AUTOFILL_FORM
};

// Returns the expected number of calls to AskForValuesToFill. On Android,
// a redundant call may be made when the focus changes to the field.
//
// Since test cases simulate multiple clicks, some of which lead to focus
// changes while others do not, finding the exact number of expected calls on
// Android is tedious. Using GMock's checkpoint pattern would help with that.
auto NumShowSuggestionsCalls() {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    return base::FeatureList::IsEnabled(
               features::kAutofillAndroidDisableSuggestionsOnJSFocus)
               // Called solely by
               // `AutofillAgent::DidReceiveLeftMouseDownOrGestureTapInNode`.
               ? testing::Exactly(1)
               // Potentially also by `AutofillAgent::FocusedElementChanged`.
               : testing::AtLeast(1);
  }
  // Called solely by `AutofillAgent::DidCompleteFocusChangeInFrame`.
  return testing::Exactly(1);
}

class PasswordAutofillAgentTest : public ChromeRenderViewTest {
 public:
  PasswordAutofillAgentTest() = default;

  PasswordAutofillAgentTest(const PasswordAutofillAgentTest&) = delete;
  PasswordAutofillAgentTest& operator=(const PasswordAutofillAgentTest&) =
      delete;

  // Simulates the fill password form message being sent to the renderer.
  // We use that so we don't have to make RenderView::OnFillPasswordForm()
  // protected.
  void SimulateOnFillPasswordForm(const PasswordFormFillData& fill_data) {
    password_autofill_agent_->SetPasswordFillData(fill_data);
  }

  void SendVisiblePasswordForms() {
    static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
        ->DidFinishLoad();
  }

  void SetUp() override {
    ChromeRenderViewTest::SetUp();

#if BUILDFLAG(IS_WIN)
    // Autofill uses the system font to render suggestion previews. On Windows
    // an extra step is required to ensure that the system font is configured.
    blink::WebFontRendering::SetMenuFontMetrics(
        blink::WebString::FromASCII("Arial"), 12);
#endif

    // TODO(crbug.com/41401202): Remove workaround preventing non-test classes
    // to bind fake_driver_ or fake_pw_client_.
    password_autofill_agent_->GetPasswordManagerDriver();
    password_generation_->RequestPasswordManagerClientForTesting();
    base::RunLoop().RunUntilIdle();  // Executes binding the interfaces.
    // Reject all requests to bind driver/client to anything but the test class:
    GetMainRenderFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PasswordGenerationDriver::Name_,
            base::BindRepeating([](mojo::ScopedInterfaceEndpointHandle handle) {
              handle.reset();
            }));
    GetMainRenderFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PasswordManagerDriver::Name_,
            base::BindRepeating([](mojo::ScopedInterfaceEndpointHandle handle) {
              handle.reset();
            }));

    // Add a preferred login and an additional login to the FillData.
    username1_ = kAliceUsername16;
    password1_ = kAlicePassword16;
    username2_ = kBobUsername16;
    password2_ = kBobPassword16;
    username3_ = kCarolUsername16;
    password3_ = kCarolPassword16;
    alternate_username3_ = kCarolAlternateUsername16;

    fill_data_.preferred_login.username_value = username1_;
    fill_data_.preferred_login.password_value = password1_;

    PasswordAndMetadata password2;
    password2.password_value = password2_;
    password2.username_value = username2_;
    fill_data_.additional_logins.push_back(std::move(password2));
    PasswordAndMetadata password3;
    password3.password_value = password3_;
    password3.username_value = username3_;
    fill_data_.additional_logins.push_back(std::move(password3));

    // We need to set the origin so it matches the frame URL, otherwise we won't
    // autocomplete.
    UpdateUrlForHTML(kFormHTML);

    LoadHTML(kFormHTML);

    // Necessary for SimulateElementClick() to work correctly.
    GetWebFrameWidget()->Resize(gfx::Size(500, 500));
    GetWebFrameWidget()->SetFocus(true);

    // Now retrieve the input elements so the test can access them.
    UpdateUsernameAndPasswordElements();
  }

  void TearDown() override {
    username_element_.Reset();
    password_element_.Reset();
    ChromeRenderViewTest::TearDown();
  }

  void RegisterMainFrameRemoteInterfaces() override {
    // Because the test cases only involve the main frame in this test,
    // the fake password client and the fake driver is only used on main frame.
    blink::AssociatedInterfaceProvider* remote_associated_interfaces =
        GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_associated_interfaces->OverrideBinderForTesting(
        mojom::PasswordGenerationDriver::Name_,
        base::BindRepeating(
            &PasswordAutofillAgentTest::BindPasswordManagerClient,
            base::Unretained(this)));
    remote_associated_interfaces->OverrideBinderForTesting(
        mojom::PasswordManagerDriver::Name_,
        base::BindRepeating(
            &PasswordAutofillAgentTest::BindPasswordManagerDriver,
            base::Unretained(this)));
  }

  void FocusElement(const std::string& element_id) {
    std::string script =
        "document.getElementById('" + element_id + "').focus()";
    ExecuteJavaScriptForTests(script.c_str());
    GetMainFrame()->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kTest);
    GetMainFrame()->Client()->FocusedElementChanged(GetElementByID(element_id));
    GetMainFrame()->AutofillClient()->DidCompleteFocusChangeInFrame();
  }

  // A workaround to focus an element that doesn't have an id attribute.
  void FocusFirstInputElement() {
    ExecuteJavaScriptForTests("document.forms[0].elements[0].focus();");
    GetMainFrame()->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kTest);
    auto first_form_element =
        GetMainFrame()->GetDocument().GetTopLevelForms()[0];
    GetMainFrame()->Client()->FocusedElementChanged(
        first_form_element.GetFormControlElements()[0]);
    GetMainFrame()->AutofillClient()->DidCompleteFocusChangeInFrame();
  }

  void BlurElement(const std::string& element_id) {
    std::string script = "document.getElementById('" + element_id + "').blur()";
    ExecuteJavaScriptForTests(script.c_str());
    ChangeFocusToNull(GetMainFrame()->GetDocument());
  }

  void ConfigurePasswordSuggestionFiltering(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          password_manager::features::kNoPasswordSuggestionFiltering);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          password_manager::features::kNoPasswordSuggestionFiltering);
    }
  }

  void EnableOverwritingPlaceholderUsernames() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kEnableOverwritingPlaceholderUsernames);
  }

  void EnableShowAutofillSignatures() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kShowAutofillSignatures);
  }

  void UpdateUrlForHTML(const std::string& html) {
    std::string url = "data:text/html;charset=utf-8," + html;
    fill_data_.url = GURL(url);
  }

  void UpdateRendererIDsInFillData() {
    fill_data_.username_element_renderer_id =
        username_element_
            ? autofill::form_util::GetFieldRendererId(username_element_)
            : autofill::FieldRendererId();

    fill_data_.password_element_renderer_id =
        password_element_
            ? autofill::form_util::GetFieldRendererId(password_element_)
            : autofill::FieldRendererId();

    ASSERT_TRUE(username_element_ || password_element_);
    WebFormElement form =
        password_element_ ? password_element_.Form() : username_element_.Form();
    fill_data_.form_renderer_id = form_util::GetFormRendererId(form);
  }

  void UpdateUsernameAndPasswordElements() {
    username_element_ = GetInputElementByID(kUsernameName);
    password_element_ = GetInputElementByID(kPasswordName);
    UpdateRendererIDsInFillData();
  }

  void UpdateOnlyUsernameElement() {
    username_element_ = GetInputElementByID(kUsernameName);
    password_element_.Reset();
    UpdateRendererIDsInFillData();
  }

  void UpdateOnlyPasswordElement() {
    username_element_.Reset();
    password_element_ = GetInputElementByID(kPasswordName);
    UpdateRendererIDsInFillData();
  }

  WebElement GetElementByID(const std::string& id) {
    WebDocument document = GetMainFrame()->GetDocument();
    WebElement element =
        document.GetElementById(WebString::FromUTF8(id.c_str()));
    EXPECT_TRUE(element);
    return element;
  }

  WebInputElement GetInputElementByID(const std::string& id) {
    WebInputElement input_element = GetElementByID(id).To<WebInputElement>();
    EXPECT_TRUE(input_element);
    return input_element;
  }

  void ClearUsernameAndPasswordFieldValues() {
    if (username_element_) {
      username_element_.SetValue(WebString());
      username_element_.SetSuggestedValue(WebString());
      username_element_.SetAutofillState(WebAutofillState::kNotFilled);
    }
    if (password_element_) {
      password_element_.SetValue(WebString());
      password_element_.SetSuggestedValue(WebString());
      password_element_.SetAutofillState(WebAutofillState::kNotFilled);
    }
  }

  void SimulateElementClick(const WebElement element) {
    SimulatePointClick(element.BoundsInWidget().CenterPoint());
  }

  using ChromeRenderViewTest::SimulateElementClick;

  void SimulateSuggestionChoice(WebInputElement& username_input) {
    std::u16string username(kAliceUsername16);
    std::u16string password(kAlicePassword16);
    SimulateSuggestionChoiceOfUsernameAndPassword(username_input, username,
                                                  password);
  }

  void SimulateSuggestionChoiceOfUsernameAndPassword(
      WebInputElement& input,
      const std::u16string& username,
      const std::u16string& password) {
    // This call is necessary to setup the autofill agent appropriate for the
    // user selection; simulates the menu actually popping up.
    SimulatePointClick(gfx::Point(1, 1));
    SimulateElementClick(input);

    password_autofill_agent_->FillPasswordSuggestion(username, password);
  }

  void SimulateUsernameTyping(const std::string& username) {
    SimulatePointClick(gfx::Point(1, 1));
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/40820173): User typing doesn't send focus events properly.
    FocusElement(kUsernameName);
#endif
    SimulateUserInputChangeForElement(username_element_, username);
  }

  void SimulatePasswordTyping(const std::string& password) {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/40820173): User typing doesn't send focus events properly.
    FocusElement(kPasswordName);
#endif
    SimulateUserInputChangeForElement(password_element_, password);
  }

  void SimulateUsernameSingleFieldAutofill(const std::u16string& text) {
    FocusElement(kUsernameName);
    autofill_agent_->ApplyFieldAction(
        mojom::FieldActionType::kReplaceAll, mojom::ActionPersistence::kFill,
        form_util::GetFieldRendererId(username_element_), text);
  }

  void SimulateUsernameFormAutofill(const std::u16string& text) {
    FocusElement(kUsernameName);
    // Fill the form.
    std::vector<autofill::FormFieldData::FillData> fields;
    FormFieldData::FillData field;
    field.value = text;
    field.is_autofilled = true;
    field.renderer_id = form_util::GetFieldRendererId(username_element_);
    field.host_form_id = form_util::GetFormRendererId(username_element_.Form());
    fields.push_back(field);

    autofill_agent_->ApplyFieldsAction(mojom::FormActionType::kFill,
                                       mojom::ActionPersistence::kFill, fields);
  }

  void SimulateUsernameFieldChange(FieldChangeSource change_source) {
    switch (change_source) {
      case FieldChangeSource::USER:
        SimulateUsernameTyping("Alice");
        break;
      case FieldChangeSource::AUTOFILL_SINGLE_FIELD:
        SimulateUsernameSingleFieldAutofill(u"Alice");
        break;
      case FieldChangeSource::USER_AUTOFILL_SINGLE_FIELD:
        SimulateUsernameTyping("A");
        SimulateUsernameSingleFieldAutofill(u"Alice");
        break;
      case FieldChangeSource::AUTOFILL_FORM:
        SimulateUsernameFormAutofill(u"Alice");
        break;
      case FieldChangeSource::USER_AUTOFILL_FORM:
        SimulateUsernameTyping("A");
        SimulateUsernameFormAutofill(u"Alice");
        break;
    }
  }

  // Helper to simulate that KeyboardReplacingSurface was closed in order to
  // test regular popups, e.g. `ShowPasswordSuggestions`.
  void SimulateClosingKeyboardReplacingSurfaceIfAndroid(
      const std::string& element_id) {
    // Put the build guard here to save space in the caller test.
#if BUILDFLAG(IS_ANDROID)
    FocusElement(element_id);
    // Don't show a keyboard, but let the caller to trigger it if needed.
    password_autofill_agent_->KeyboardReplacingSurfaceClosed(
        /*show_virtual_keyboard*/ false);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // TODO(crbug.com/40278548): Only expect one of IsPreviewed()/IsAutofilled().
  void CheckTextFieldsStateForElements(const WebInputElement& username_element,
                                       const std::string& username,
                                       bool username_autofilled,
                                       const WebInputElement& password_element,
                                       const std::string& password,
                                       bool password_autofilled,
                                       bool check_suggested_username,
                                       bool check_suggested_password) {
    if (username_element) {
      EXPECT_EQ(username, check_suggested_username
                              ? username_element.SuggestedValue().Utf8()
                              : username_element.Value().Utf8())
          << "check_suggested_username == " << check_suggested_username;
      EXPECT_EQ(username_autofilled, username_element.IsPreviewed() ||
                                         username_element.IsAutofilled());
    }

    if (password_element) {
      EXPECT_EQ(password, check_suggested_password
                              ? password_element.SuggestedValue().Utf8()
                              : password_element.Value().Utf8())
          << "check_suggested_password == " << check_suggested_password;
      EXPECT_EQ(password_autofilled, password_element.IsAutofilled() ||
                                         password_element.IsPreviewed());
    }
  }

  // Checks the DOM-accessible value of the username element and the
  // *suggested* value of the password element.
  void CheckUsernameDOMStatePasswordSuggestedState(const std::string& username,
                                                   bool username_autofilled,
                                                   const std::string& password,
                                                   bool password_autofilled) {
    CheckTextFieldsStateForElements(
        username_element_, username, username_autofilled, password_element_,
        password, password_autofilled, false /* check_suggested_username */,
        true /* check_suggested_password */);
  }

  // Checks the DOM-accessible value of the username element and the
  // DOM-accessible value of the password element.
  void CheckTextFieldsDOMState(const std::string& username,
                               bool username_autofilled,
                               const std::string& password,
                               bool password_autofilled) {
    CheckTextFieldsStateForElements(
        username_element_, username, username_autofilled, password_element_,
        password, password_autofilled, false /* check_suggested_username */,
        false /* check_suggested_password */);
  }

  // Checks the suggested values of the `username` and `password` elements.
  void CheckTextFieldsSuggestedState(const std::string& username,
                                     bool username_autofilled,
                                     const std::string& password,
                                     bool password_autofilled) {
    CheckTextFieldsStateForElements(
        username_element_, username, username_autofilled, password_element_,
        password, password_autofilled, true /* check_suggested_username */,
        true /* check_suggested_password */);
  }

  void ResetFieldState(
      WebInputElement* element,
      const std::string& value = std::string(),
      blink::WebAutofillState is_autofilled = WebAutofillState::kNotFilled) {
    element->SetValue(WebString::FromUTF8(value));
    element->SetSuggestedValue(WebString());
    element->SetAutofillState(is_autofilled);
    element->SetSelectionRange(value.size(), value.size());
  }

  void CheckUsernameSelection(unsigned start, unsigned end) {
    EXPECT_EQ(start, username_element_.SelectionStart());
    EXPECT_EQ(end, username_element_.SelectionEnd());
  }

  // Checks the message sent to PasswordAutofillManager to build the suggestion
  // list. `typed_username` is the expected username field value, and `show_all`
  // is the expected flag for the PasswordAutofillManager, whether to show all
  // suggestions, or only those starting with `typed_username`.
  void CheckSuggestions(const std::u16string& typed_username,
                        bool show_all,
                        base::Location location = FROM_HERE) {
    std::u16string expected_username = show_all ? u"" : typed_username;
    SCOPED_TRACE(testing::Message()
                 << __func__ << " called from " << location.ToString());
    EXPECT_CALL(fake_driver_,
                ShowPasswordSuggestions(AllOf(
                    Field(&autofill::PasswordSuggestionRequest::typed_username,
                          expected_username))))
        .Times(NumShowSuggestionsCalls());
    base::RunLoop().RunUntilIdle();
  }

  void CheckSuggestionsNotShown() {
    EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
    base::RunLoop().RunUntilIdle();
  }

  void ExpectFieldPropertiesMasks(
      PasswordFormSourceType expected_type,
      const std::map<std::u16string, FieldPropertiesMask>&
          expected_properties_masks,
      autofill::mojom::SubmissionIndicatorEvent expected_submission_event) {
    base::RunLoop().RunUntilIdle();
    autofill::FormData form_data;
    if (expected_type == PasswordFormSubmitted) {
      ASSERT_TRUE(fake_driver_.called_password_form_submitted());
      ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_submitted()));
      form_data = *(fake_driver_.form_data_submitted());
    } else {
      ASSERT_EQ(PasswordFormSameDocumentNavigation, expected_type);
      ASSERT_TRUE(fake_driver_.called_dynamic_form_submission());
      ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_maybe_submitted()));
      form_data = *(fake_driver_.form_data_maybe_submitted());
      EXPECT_EQ(expected_submission_event, form_data.submission_event());
    }

    size_t unchecked_masks = expected_properties_masks.size();
    for (const FormFieldData& field : form_data.fields()) {
      const auto& it = expected_properties_masks.find(field.name());
      if (it == expected_properties_masks.end())
        continue;
      EXPECT_EQ(field.properties_mask(), it->second)
          << "Wrong mask for the field " << field.name();
      unchecked_masks--;
    }
    EXPECT_TRUE(unchecked_masks == 0)
        << "Some expected masks are missed in FormData";
  }

  FormRendererId GetFormUniqueRendererId(const WebString& form_id) {
    WebLocalFrame* frame = GetMainFrame();
    if (!frame)
      return FormRendererId();
    WebFormElement web_form =
        frame->GetDocument().GetElementById(form_id).To<WebFormElement>();
    return form_util::GetFormRendererId(web_form);
  }

  void ExpectFormDataWithUsernameAndPasswordsAndEvent(
      const autofill::FormData& form_data,
      FormRendererId form_renderer_id,
      const std::u16string& username_value,
      const std::u16string& password_value,
      const std::u16string& new_password_value,
      SubmissionIndicatorEvent event) {
    EXPECT_EQ(form_renderer_id, form_data.renderer_id());
    EXPECT_TRUE(FormHasFieldWithValue(form_data, username_value));
    EXPECT_TRUE(FormHasFieldWithValue(form_data, password_value));
    EXPECT_TRUE(FormHasFieldWithValue(form_data, new_password_value));
    EXPECT_EQ(form_data.submission_event(), event);
  }

  void ExpectFormSubmittedWithUsernameAndPasswords(
      FormRendererId form_renderer_id,
      const std::u16string& username_value,
      const std::u16string& password_value,
      const std::u16string& new_password_value) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(fake_driver_.called_password_form_submitted());
    ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_submitted()));
    ExpectFormDataWithUsernameAndPasswordsAndEvent(
        *(fake_driver_.form_data_submitted()), form_renderer_id, username_value,
        password_value, new_password_value,
        SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  }

  void ExpectSameDocumentNavigationWithUsernameAndPasswords(
      FormRendererId form_renderer_id,
      const std::u16string& username_value,
      const std::u16string& password_value,
      const std::u16string& new_password_value,
      SubmissionIndicatorEvent event) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(fake_driver_.called_dynamic_form_submission());
    ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_maybe_submitted()));
    ExpectFormDataWithUsernameAndPasswordsAndEvent(
        *(fake_driver_.form_data_maybe_submitted()), form_renderer_id,
        username_value, password_value, new_password_value, event);
  }

  void CheckIfEventsAreCalled(const std::vector<std::u16string>& checkers,
                              bool expected) {
    for (const std::u16string& variable : checkers) {
      int value;
      EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(variable, &value))
          << variable;
      EXPECT_EQ(expected, value == 1) << variable;
    }
  }

  void BindPasswordManagerDriver(mojo::ScopedInterfaceEndpointHandle handle) {
    fake_driver_.BindReceiver(
        mojo::PendingAssociatedReceiver<mojom::PasswordManagerDriver>(
            std::move(handle)));
  }

  void BindPasswordManagerClient(mojo::ScopedInterfaceEndpointHandle handle) {
    fake_pw_client_.BindReceiver(
        mojo::PendingAssociatedReceiver<mojom::PasswordGenerationDriver>(
            std::move(handle)));
  }

  void SaveAndSubmitForm() { SaveAndSubmitForm(username_element_.Form()); }

  void SaveAndSubmitForm(const WebFormElement& form_element) {
    FormTracker& tracker = test_api(*autofill_agent_).form_tracker();
    static_cast<blink::WebLocalFrameObserver&>(tracker).WillSendSubmitEvent(
        form_element);
    static_cast<content::RenderFrameObserver&>(tracker).WillSubmitForm(
        form_element);
  }

  void CheckFirstFillingResult(FillingResult result) {
    histogram_tester_.ExpectUniqueSample(
        "PasswordManager.FirstRendererFillingResult", result, 1);
  }

  void SubmitForm() {
    FormTracker& tracker = test_api(*autofill_agent_).form_tracker();
    static_cast<content::RenderFrameObserver&>(tracker).WillSubmitForm(
        username_element_.Form());
  }

  void FireAjaxSucceeded() {
    FormTracker& tracker = test_api(*autofill_agent_).form_tracker();
    tracker.AjaxSucceeded();
  }

  void FireDidFinishSameDocumentNavigation() {
    FormTracker& tracker = test_api(*autofill_agent_).form_tracker();
    static_cast<content::RenderFrameObserver&>(tracker)
        .DidFinishSameDocumentNavigation();
  }

  ::testing::AssertionResult UpdateFormElementsForFormHostingShadowDom() {
    username_element_ = GetElementByID("un_host")
                            .ShadowRoot()
                            .FirstChild()
                            .To<WebInputElement>();
    if (!username_element_) {
      return ::testing::AssertionFailure() << "Username element is null.";
    }
    password_element_ = GetElementByID("pw_host")
                            .ShadowRoot()
                            .FirstChild()
                            .To<WebInputElement>();
    if (!password_element_) {
      return ::testing::AssertionFailure() << "Password element is null.";
    }
    return ::testing::AssertionSuccess();
  }

  // This triggers a layout update to apply JS changes like display = 'none'.
  void ForceLayoutUpdate() {
    GetWebFrameWidget()->UpdateAllLifecyclePhases(
        blink::DocumentUpdateReason::kTest);
  }

  FakeMojoPasswordManagerDriver fake_driver_;
  testing::NiceMock<FakePasswordGenerationDriver> fake_pw_client_;

  std::u16string username1_;
  std::u16string username2_;
  std::u16string username3_;
  std::u16string password1_;
  std::u16string password2_;
  std::u16string password3_;
  std::u16string alternate_username3_;
  PasswordFormFillData fill_data_;

  WebInputElement username_element_;
  WebInputElement password_element_;
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests that the password login is autocompleted as expected when the browser
// sends back the password info.
TEST_F(PasswordAutofillAgentTest, InitialAutocomplete) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Tests that we correctly fill forms having an empty 'action' attribute.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForEmptyAction) {
  const char kEmptyActionFormHTML[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kEmptyActionFormHTML);

  // Retrieve the input elements so the test can access them.
  UpdateUsernameAndPasswordElements();

  // Set the expected form origin.
  UpdateUrlForHTML(kEmptyActionFormHTML);

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that if a password is marked as readonly, neither field is autofilled
// on page load.
TEST_F(PasswordAutofillAgentTest, NoInitialAutocompleteForReadOnlyPassword) {
  SetElementReadOnly(password_element_, true);

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState(std::string(), false, std::string(), false);

  CheckFirstFillingResult(FillingResult::kPasswordElementIsNotAutocompleteable);
}

// Can still fill a password field if the username is set to a value that
// matches.
TEST_F(PasswordAutofillAgentTest,
       AutocompletePasswordForReadonlyUsernameMatched) {
  username_element_.SetValue(WebString::FromUTF16(username3_));
  SetElementReadOnly(username_element_, true);

  // Filled even though username is not the preferred match.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(username3_), false,
                                              UTF16ToUTF8(password3_), true);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Fill username and password fields when username field contains a prefilled
// value that matches the list of known possible prefilled values usually used
// as placeholders.
TEST_F(PasswordAutofillAgentTest, AutocompleteForPrefilledUsernameValue) {
  // Set the username element to a value from the prefilled values list.
  // Comparison should be insensitive to leading and trailing whitespaces.
  username_element_.SetValue(WebString::FromUTF16(u" User Name "));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should both have suggested values.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  // Simulate a user click so that the password field's real value is filled.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));

  // The username and password should have been autocompleted.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PrefilledUsernameFillOutcome",
      PrefilledUsernameFillOutcome::kPrefilledPlaceholderUsernameOverridden, 1);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Tests that if filling is invoked twice for the same autofill agent the
// prefilled username and first filling metrics are only logged once.
TEST_F(PasswordAutofillAgentTest, MetricsOnlyLoggedOnce) {
  // Set the username element to a value from the prefilled values list.
  // Comparison should be insensitive to leading and trailing whitespaces.
  username_element_.SetValue(WebString::FromUTF16(u" User Name "));

  // Simulate the browser sending back the login info multiple times.
  // This triggers the autocomplete.
  SimulateOnFillPasswordForm(fill_data_);
  SimulateOnFillPasswordForm(fill_data_);

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PrefilledUsernameFillOutcome",
      PrefilledUsernameFillOutcome::kPrefilledPlaceholderUsernameOverridden, 1);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Fill a password field if the stored username is a prefix of username in
// read-only field.
TEST_F(PasswordAutofillAgentTest,
       AutocompletePasswordForReadonlyUsernamePrefixMatched) {
  std::u16string username_at = username3_ + u"@example.com";
  username_element_.SetValue(WebString::FromUTF16(username_at));
  SetElementReadOnly(username_element_, true);

  // Filled even though the username in the form is only a proper prefix of the
  // stored username.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(username_at), false,
                                              UTF16ToUTF8(password3_), true);
}

// Credentials are sent to the renderer even for sign-up forms as these may be
// eligible for filling via manual fall back. In this case, the username_field
// and password_field are not set. This test verifies that no failures are
// recorded in PasswordManager.FirstRendererFillingResult.
TEST_F(PasswordAutofillAgentTest, NoFillingOnSignupForm_NoMetrics) {
  LoadHTML(kSignupFormHTML);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("random_info"));
  ASSERT_TRUE(element);
  username_element_ = element.To<WebInputElement>();

  fill_data_.username_element_renderer_id = autofill::FieldRendererId();
  fill_data_.password_element_renderer_id = autofill::FieldRendererId();

  WebFormElement form_element =
      document.GetElementById("LoginTestForm").To<WebFormElement>();
  fill_data_.form_renderer_id = form_util::GetFormRendererId(form_element);

  SimulateOnFillPasswordForm(fill_data_);
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.FirstRendererFillingResult", 0);
}

// Do not fill a password field if the stored username is a prefix without @
// of username in read-only field.
TEST_F(PasswordAutofillAgentTest,
       DontAutocompletePasswordForReadonlyUsernamePrefixMatched) {
  std::u16string prefilled_username = username3_ + u"example.com";
  username_element_.SetValue(WebString::FromUTF16(prefilled_username));
  SetElementReadOnly(username_element_, true);

  // Filled even though the username in the form is only a proper prefix of the
  // stored username.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(prefilled_username),
                                              false, std::string(), false);

  CheckFirstFillingResult(
      FillingResult::kUsernamePrefilledWithIncompatibleValue);
}

// Do not fill a password field if the field isn't readonly despite the stored
// username is a prefix without @ of username in read-only field.
TEST_F(
    PasswordAutofillAgentTest,
    DontAutocompletePasswordForNotReadonlyUsernameFieldEvenWhenPrefixMatched) {
  std::u16string prefilled_username = username3_ + u"@example.com";
  username_element_.SetValue(WebString::FromUTF16(prefilled_username));

  // Filled even though the username in the form is only a proper prefix of the
  // stored username.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(prefilled_username),
                                              false, std::string(), false);
}

// If a username field is empty and readonly, don't autofill.
TEST_F(PasswordAutofillAgentTest,
       NoAutocompletePasswordForReadonlyUsernameUnmatched) {
  username_element_.SetValue(WebString::FromUTF8(""));
  SetElementReadOnly(username_element_, true);

  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(std::string(), false,
                                              std::string(), false);

  CheckFirstFillingResult(FillingResult::kFoundNoPasswordForUsername);
}

// Tests that having a non-matching username precludes the autocomplete.
TEST_F(PasswordAutofillAgentTest, NoAutocompleteForFilledFieldUnmatched) {
  username_element_.SetValue(WebString::FromUTF8("bogus"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckUsernameDOMStatePasswordSuggestedState("bogus", false, std::string(),
                                              false);

  CheckFirstFillingResult(
      FillingResult::kUsernamePrefilledWithIncompatibleValue);
}

// Don't try to complete a prefilled value that is a partial match
// to a username if the prefilled value isn't on the list of known values
// used as placeholders.
TEST_F(PasswordAutofillAgentTest, NoPartialMatchForPrefilledUsername) {
  username_element_.SetValue(WebString::FromUTF8("ali"));

  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState("", false, std::string(), false);
  CheckUsernameDOMStatePasswordSuggestedState("ali", false, std::string(),
                                              false);

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PrefilledUsernameFillOutcome",
      autofill::PrefilledUsernameFillOutcome::kPrefilledUsernameNotOverridden,
      1);
}

// Tests that having a matching username precludes the autofill.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForMatchingFilledField) {
  username_element_.SetValue(WebString::FromUTF16(kAliceUsername16));

  // Simulate the browser sending back the login info, it triggers the
  // autofill.
  SimulateOnFillPasswordForm(fill_data_);

  // The password should have been autofilled, but the username field should
  // have been left alone, since it contained the correct value already.
  CheckUsernameDOMStatePasswordSuggestedState(kAliceUsername, false,
                                              kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

TEST_F(PasswordAutofillAgentTest, PasswordNotClearedOnEdit) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameTyping("alicia");

  // The password should not have been cleared.
  CheckTextFieldsDOMState("alicia", false, kAlicePassword, true);
}

// Tests that lost focus does not trigger filling when `wait_for_username` is
// true.
TEST_F(PasswordAutofillAgentTest, WaitUsername) {
  // Simulate the browser sending back the login info.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Ensure TTF isn't in the foreground while this test simulates a typing user.
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  // No auto-fill should have taken place.
  CheckTextFieldsSuggestedState(
      /*username=*/std::string(),
      /*username_autofilled=*/false,
      /*password=*/std::string(),
      /*password_autofilled=*/false);

  SimulateUsernameTyping(kAliceUsername);
  // Change focus in between to make sure blur events don't trigger filling.
  SetFocused(password_element_);
  SetFocused(username_element_);

  // No autocomplete should happen when text is entered in the username.
  CheckUsernameDOMStatePasswordSuggestedState(
      /*username=*/kAliceUsername,
      /*username_autofilled=*/false,
      /*password=*/std::string(),
      /*password_autofilled=*/false);

  CheckFirstFillingResult(FillingResult::kWaitForUsername);
}

TEST_F(PasswordAutofillAgentTest, IsWebElementVisibleTest) {
  blink::WebLocalFrame* frame;

  LoadHTML(kVisibleFormWithNoUsernameHTML);
  frame = GetMainFrame();
  blink::WebVector<WebFormElement> forms =
      frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1u, forms.size());
  blink::WebVector<blink::WebFormControlElement> web_control_elements =
      forms[0].GetFormControlElements();
  ASSERT_EQ(1u, web_control_elements.size());
  EXPECT_TRUE(
      form_util::IsWebElementFocusableForAutofill(web_control_elements[0]));

  LoadHTML(kNonVisibleFormHTML);
  frame = GetMainFrame();
  forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1u, forms.size());
  web_control_elements = forms[0].GetFormControlElements();
  ASSERT_EQ(1u, web_control_elements.size());
  EXPECT_FALSE(
      form_util::IsWebElementFocusableForAutofill(web_control_elements[0]));

  LoadHTML(kNonDisplayedFormHTML);
  frame = GetMainFrame();
  forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1u, forms.size());
  web_control_elements = forms[0].GetFormControlElements();
  ASSERT_EQ(1u, web_control_elements.size());
  EXPECT_FALSE(
      form_util::IsWebElementFocusableForAutofill(web_control_elements[0]));
}

TEST_F(PasswordAutofillAgentTest,
       SendPasswordFormsTest_VisibleFormWithNoUsername) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_FALSE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_EmptyForm) {
  base::RunLoop().RunUntilIdle();
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kEmptyFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_FormWithoutPasswords) {
  base::RunLoop().RunUntilIdle();
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kFormWithoutPasswordsHTML);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());

  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest,
       SendPasswordFormsTest_UndetectedPasswordField) {
  base::RunLoop().RunUntilIdle();
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kFormWithoutPasswordsHTML);
  // Emulate that a password field appears but we don't detect that.
  std::string script =
      "document.getElementById('random_field').type = 'password';";
  ExecuteJavaScriptForTests(script.c_str());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());

  // When the user clicks on the field, a request to the store will be sent.
  EXPECT_TRUE(SimulateElementClick("random_field"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());

  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_NonDisplayedForm) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kNonDisplayedFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_NonVisibleForm) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kNonVisibleFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_PasswordChangeForm) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kPasswordChangeFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_FALSE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest,
       SendPasswordFormsTest_CannotCreatePasswordForm) {
  // This test checks that a request to the store is sent even if it is a credit
  // card form.
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kCreditCardFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_FALSE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_ReloadTab) {
  // PasswordAutofillAgent::sent_request_to_store_ disables duplicate requests
  // to the store. This test checks that new request will be sent if the frame
  // has been reloaded.
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kNonVisibleFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());

  fake_driver_.reset_password_forms_calls();
  std::string url_string = "data:text/html;charset=utf-8,";
  url_string.append(kNonVisibleFormHTML);
  Reload(GURL(url_string));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_Redirection) {
  base::RunLoop().RunUntilIdle();

  fake_driver_.reset_password_forms_calls();
  LoadHTML(kEmptyWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_calls();
  LoadHTML(kRedirectionWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_calls();
  LoadHTML(kSimpleWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_calls();
  LoadHTML(kWebpageWithDynamicContent);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
}

// Tests that fields that are not under a <form> tag are only sent to
// PasswordManager if they contain a password field.
TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_UnownedtextInputs) {
  fake_driver_.reset_password_forms_calls();
  const char kFormlessFieldsNonPasswordHTML[] =
      "  <INPUT type='text' name='email'>"
      "  <INPUT type='submit' value='Login'/>";
  LoadHTML(kFormlessFieldsNonPasswordHTML);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_password_forms_parsed());

  fake_driver_.reset_password_forms_calls();
  const char kFormlessFieldsPasswordHTML[] =
      "  <INPUT type='text' name='email'>"
      "  <INPUT type='password' name='pw'>"
      "  <INPUT type='submit' value='Login'/>";
  LoadHTML(kFormlessFieldsPasswordHTML);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_driver_.called_password_forms_parsed());
}

// Tests that a password will only be filled as a suggested and will not be
// accessible by the DOM until a user gesture has occurred.
TEST_F(PasswordAutofillAgentTest, GestureRequiredTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  // However, it should only have completed with the suggested value, as tested
  // above, and it should not have completed into the DOM accessible value for
  // the password field.
  CheckTextFieldsDOMState(std::string(), true, std::string(), true);

  // Simulate a user click so that the password field's real value is filled.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Verifies that a DOM-activated UI event will not cause an autofill.
TEST_F(PasswordAutofillAgentTest, NoDOMActivationTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  ExecuteJavaScriptForTests(kJavaScriptClick);
  CheckTextFieldsDOMState("", true, "", true);
}

// Verifies that password autofill triggers events in JavaScript for forms that
// are filled on page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsOnLoad) {
  std::vector<std::u16string> username_event_checkers;
  std::vector<std::u16string> password_event_checkers;
  std::string events_registration_script =
      CreateScriptToRegisterListeners(kUsernameName, &username_event_checkers) +
      CreateScriptToRegisterListeners(kPasswordName, &password_event_checkers);
  std::string html = std::string(kFormHTML) + events_registration_script;
  LoadHTML(html.c_str());
  UpdateUrlForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted...
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
  // ... but since there hasn't been a user gesture yet, the autocompleted
  // username and password should only be visible to the user.
  CheckTextFieldsDOMState(std::string(), true, std::string(), true);

  // JavaScript events shouldn't have been triggered for the username and the
  // password yet.
  CheckIfEventsAreCalled(username_event_checkers, false);
  CheckIfEventsAreCalled(password_event_checkers, false);

  // Simulate a user click so that the password field's real value is filled.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // Now, JavaScript events should have been triggered.
  CheckIfEventsAreCalled(username_event_checkers, true);
  CheckIfEventsAreCalled(password_event_checkers, true);
}

// Verifies that password autofill triggers events in JavaScript for forms that
// are filled after page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsWaitForUsername) {
  std::vector<std::u16string> event_checkers;
  std::string events_registration_script =
      CreateScriptToRegisterListeners(kUsernameName, &event_checkers) +
      CreateScriptToRegisterListeners(kPasswordName, &event_checkers);
  std::string html = std::string(kFormHTML) + events_registration_script;
  LoadHTML(html.c_str());
  UpdateUrlForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should not yet have been autocompleted.
  CheckTextFieldsSuggestedState(std::string(), false, std::string(), false);

  // Simulate a click just to force a user gesture, since the username value is
  // set directly.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));

  // Simulate the user entering the first letter of their username and selecting
  // the matching autofill from the dropdown.
  SimulateUsernameTyping("a");
  // Since the username element has focus, blur event will be not triggered.
  std::erase(event_checkers, u"username_blur_event");
  SimulateSuggestionChoice(username_element_);

  // The username and password should now have been autocompleted.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // JavaScript events should have been triggered both for the username and for
  // the password.
  CheckIfEventsAreCalled(event_checkers, true);
}

// Tests that `FillSuggestion` properly fills the username and password on
// focused `username_element_`.
TEST_F(PasswordAutofillAgentTest, FillSuggestionOnUsernameField) {
  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  SimulateElementClick(username_element_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(/*username=*/std::string(),
                          /*username_autofilled=*/false,
                          /*password=*/std::string(),
                          /*password_autofilled=*/false);

  // If the username field is not autocompletable, no element will be filled.
  SetElementReadOnly(username_element_, /*read_only=*/true);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(/*username=*/std::string(),
                          /*username_autofilled=*/false,
                          /*password=*/std::string(),
                          /*password_autofilled=*/false);
  SetElementReadOnly(username_element_, /*read_only=*/false);

  // If the password field is not autocompletable, only username will be filled.
  SetElementReadOnly(password_element_, /*read_only=*/true);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(kAliceUsername, /*username_autofilled=*/true,
                          /*password=*/std::string(),
                          /*password_autofilled=*/false);
  size_t username_length = strlen(kAliceUsername);
  CheckUsernameSelection(username_length, username_length);
  SetElementReadOnly(password_element_, /*read_only=*/false);
  ResetFieldState(&username_element_);

  // After filling with the suggestion, both fields should be autocompleted.
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(kAliceUsername, /*username_autofilled=*/true,
                          kAlicePassword, /*password_autofilled=*/true);
  username_length = strlen(kAliceUsername);
  CheckUsernameSelection(username_length, username_length);

  // Try filling with a suggestion with password different from the one that
  // was initially sent to the renderer.
  password_autofill_agent_->FillPasswordSuggestion(kBobUsername16,
                                                   kCarolPassword16);
  CheckTextFieldsDOMState(kBobUsername, /*username_autofilled=*/true,
                          kCarolPassword, /*password_autofilled=*/true);
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(username_length, username_length);
}

// Avoid filling suggestion on username if the password field is disabled and
// there is no <form> tag.
TEST_F(PasswordAutofillAgentTest,
       NoFillSuggestionOnNoFormTagAndPasswordDisabled) {
  LoadHTML(kUnownedFieldsWithPasswordDisabled);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  SimulateElementClick(username_element_);

  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);
}

// Tests that `FillSuggestion` properly fills the username and password on
// focused `password_element_`.
TEST_F(PasswordAutofillAgentTest, FillSuggestionOnPasswordField) {
  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  SimulateElementClick(password_element_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);

  // If the password field is not autocompletable, no filling will be made.
  SetElementReadOnly(password_element_, /*read_only=*/true);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);
  SetElementReadOnly(password_element_, /*read_only=*/false);

  // If the username field is not autocompletable, only password field will be
  // filled.
  SetElementReadOnly(username_element_, /*read_only=*/true);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(/*username=*/std::string(),
                          /*username_autofilled=*/false, kAlicePassword,
                          /*password_autofilled=*/true);
  SetElementReadOnly(username_element_, /*read_only=*/false);
  ResetFieldState(&username_element_);

  // After filling with the suggestion, both fields should be autocompleted.
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(kAliceUsername, /*username_autofilled=*/true,
                          kAlicePassword, /*password_autofilled=*/true);
  size_t username_length = strlen(kAliceUsername);
  CheckUsernameSelection(username_length, username_length);

  // Try filling with a suggestion with password different from the one that
  // was initially sent to the renderer.
  password_autofill_agent_->FillPasswordSuggestion(kBobUsername16,
                                                   kCarolPassword16);
  CheckTextFieldsDOMState(kBobUsername, /*username_autofilled=*/true,
                          kCarolPassword, /*password_autofilled=*/true);
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(username_length, username_length);
}

// Tests that `FillSuggestion` properly fills the username and password when the
// username field is created dynamically in JavaScript.
TEST_F(PasswordAutofillAgentTest, FillSuggestionWithDynamicUsernameField) {
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  constexpr const char* kAddUsernameToFormScript =
      "var new_input = document.createElement('input');"
      "new_input.setAttribute('type', 'text');"
      "new_input.setAttribute('id', 'username');"
      "password_field = document.getElementById('password');"
      "password_field.parentNode.insertBefore(new_input, password_field);";
  ExecuteJavaScriptForTests(kAddUsernameToFormScript);
  UpdateUsernameAndPasswordElements();
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // After filling with the suggestion, both fields should be autocompleted.
  SimulateElementClick(password_element_);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);

  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kWaitForUsername);
}

// Tests that `FillSuggestion` doesn't change non-empty non-autofilled username
// when interacting with the password field.
TEST_F(PasswordAutofillAgentTest,
       FillSuggestionFromPasswordFieldWithUsernameManuallyFilled) {
  username_element_.SetValue(WebString::FromUTF8("user1"));

  // Simulate the browser sending the login info, but set `wait_for_username` to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState("user1", false, std::string(), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, fake_driver_.called_inform_about_user_input_count());

  // Only password field should be autocompleted.
  SimulateElementClick(password_element_);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState("user1", false, kAlicePassword, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_inform_about_user_input_count());

  // Try Filling with a different password. Only password should be changed.
  password_autofill_agent_->FillPasswordSuggestion(kBobUsername16,
                                                   kCarolPassword16);
  CheckTextFieldsDOMState("user1", false, kCarolPassword, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_driver_.called_inform_about_user_input_count());
}

// Tests that `PreviewSuggestion` properly previews the username and password on
// `username_element_` focus.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionOnUsernameField) {
  // Simulate the browser sending the login info, but set `wait_for_username` to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);

  // If the password field is not autocompletable, the preview must be available
  // only on username.
  SetElementReadOnly(password_element_, /*read_only=*/true);
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(kAliceUsername, /*username_autofilled=*/true,
                                /*password=*/std::string(),
                                /*password_autofilled=*/false);
  SetElementReadOnly(password_element_, /*read_only=*/false);
  password_autofill_agent_->ClearPreviewedForm();

  // If the username field is not autocompletable, the preview must not be shown
  // on any field.
  SetElementReadOnly(username_element_, /*read_only=*/true);
  password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);
  SetElementReadOnly(username_element_, /*read_only=*/false);

  // After selecting the preview, both fields should be previewed with
  // suggested values.
  password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(kAliceUsername, /*username_autofilled=*/true,
                                kAlicePassword, /*password_autofilled=*/true);
  // Since the suggestion is previewed as a placeholder, there should be no
  // selected text.
  CheckUsernameSelection(/*start=*/0, /*end=*/0);

  // Try previewing with a password different from the one that was initially
  // sent to the renderer.
  password_autofill_agent_->PreviewSuggestion(username_element_, kBobUsername16,
                                              kCarolPassword16);
  CheckTextFieldsSuggestedState(kBobUsername, /*username_autofilled=*/true,
                                kCarolPassword, /*password_autofilled=*/true);
  // Since the suggestion is previewed as a placeholder, there should be no
  // selected text.
  CheckUsernameSelection(/*start=*/0, /*end=*/0);
}

// Tests that `PreviewSuggestion` properly previews the username and password on
// `password_element_` focus.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionOnPasswordField) {
  // Simulate the browser sending the login info, but set `wait_for_username` to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(/*username=*/std::string(),
                          /*username_autofilled=*/false,
                          /*password=*/std::string(),
                          /*password_autofilled=*/false);

  // If the password field is not autocompletable, there must be no preview
  // available.
  SetElementReadOnly(password_element_, /*read_only=*/true);
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  password_autofill_agent_->PreviewSuggestion(
      password_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);
  SetElementReadOnly(password_element_, /*read_only=*/false);

  // If the username field is not autocompletable, the preview must be shown
  // only on the password field.
  SetElementReadOnly(username_element_, /*read_only=*/true);
  password_autofill_agent_->PreviewSuggestion(
      password_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(/*username=*/std::string(),
                                /*username_autofilled=*/false, kAlicePassword,
                                /*password_autofilled=*/true);
  SetElementReadOnly(username_element_, /*read_only=*/false);

  // After previewing the suggestion, both fields should be previewed with
  // suggested values.
  password_autofill_agent_->PreviewSuggestion(
      password_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(kAliceUsername, /*username_autofilled=*/true,
                                kAlicePassword, /*password_autofilled=*/true);
  // Since the suggestion is previewed as a placeholder, there should be no
  // selected text.
  CheckUsernameSelection(/*start=*/0, /*end=*/0);

  // Try previewing with a password different from the one that was initially
  // sent to the renderer.
  password_autofill_agent_->PreviewSuggestion(password_element_, kBobUsername16,
                                              kCarolPassword16);
  CheckTextFieldsSuggestedState(kBobUsername, /*username_autofilled=*/true,
                                kCarolPassword, /*password_autofilled=*/true);
  // Since the suggestion is previewed as a placeholder, there should be no
  // selected text.
  CheckUsernameSelection(/*start=*/0, /*end=*/0);
}

// Tests that `PreviewSuggestion` doesn't change non-empty non-autofilled
// username when previewing autofills on interacting with the password field.
TEST_F(PasswordAutofillAgentTest,
       PreviewSuggestionFromPasswordFieldWithUsernameManuallyFilled) {
  username_element_.SetValue(WebString::FromUTF8("user1"));

  // Simulate the browser sending the login info, but set `wait_for_username` to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState("user1", false, std::string(), false);

  // Only password field should be autocompleted.
  ASSERT_TRUE(SimulateElementClick("password"));
  password_autofill_agent_->PreviewSuggestion(
      password_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(std::string(), false, kAlicePassword, true);
  CheckTextFieldsDOMState("user1", false, std::string(), true);

  // Try previewing with a different password. Only password should be changed.
  password_autofill_agent_->PreviewSuggestion(password_element_, kBobUsername16,
                                              kCarolPassword16);
  CheckTextFieldsSuggestedState(std::string(), false, kCarolPassword, true);
  CheckTextFieldsDOMState("user1", false, std::string(), true);
}

// Tests that `PreviewSuggestion` properly sets the username selection range.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionSelectionRange) {
  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    ASSERT_TRUE(
        SimulateElementClick(selected_element.GetAttribute("id").Ascii()));
    ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
    ResetFieldState(&password_element_);

    password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername16, kAlicePassword16);
    CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
    // The selection should be set after the third character.
    CheckUsernameSelection(3, 3);
  }
}

// Tests that `ClearPreview` properly clears previewed username and password
// with password being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithPasswordAutofilled) {
  ResetFieldState(&password_element_, "sec", WebAutofillState::kPreviewed);

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState(std::string(), false, "sec", true);

  for (const auto& selected_element : {username_element_, password_element_}) {
    ASSERT_TRUE(
        SimulateElementClick(selected_element.GetAttribute("id").Ascii()));
    password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername16, kAlicePassword16);
    password_autofill_agent_->ClearPreviewedForm();

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState(std::string(), false, "sec", true);
    CheckUsernameSelection(0, 0);
  }
}

// Tests that `ClearPreview` properly clears previewed username and password
// with username being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithUsernameAutofilled) {
  ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
  username_element_.SetSelectionRange(3, 3);

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  for (const auto& selected_element : {username_element_, password_element_}) {
    ASSERT_TRUE(
        SimulateElementClick(selected_element.GetAttribute("id").Ascii()));
    password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername16, kAlicePassword16);
    password_autofill_agent_->ClearPreviewedForm();

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState("ali", true, std::string(), false);
    CheckUsernameSelection(3, 3);
  }
}

// Tests that `PreviewField` correctly previews fields.
TEST_F(PasswordAutofillAgentTest, PreviewField) {
  WebInputElement random_element = GetInputElementByID("random_field");
  std::vector<WebInputElement> elements{username_element_, password_element_,
                                        random_element};
  for (WebInputElement& element : elements) {
    SetElementReadOnly(element, true);
    password_autofill_agent_->PreviewField(
        form_util::GetFieldRendererId(element), kAliceUsername16);
    EXPECT_TRUE(element.SuggestedValue().IsEmpty());

    SetElementReadOnly(element, false);
    password_autofill_agent_->PreviewField(
        form_util::GetFieldRendererId(element), kAliceUsername16);
    EXPECT_EQ(kAliceUsername, element.SuggestedValue().Utf8());
  }
}

// Tests that the field state is correctly reset after preview.
TEST_F(PasswordAutofillAgentTest, PreviewField_ClearPreviewedForm) {
  WebInputElement random_element = GetInputElementByID("random_field");
  std::vector<WebInputElement> elements{username_element_, password_element_,
                                        random_element};
  for (WebInputElement& element : elements) {
    // Simulate autofilling the field with "ali".
    ResetFieldState(&element, "ali", WebAutofillState::kAutofilled);
    element.SetSelectionRange(0u, 0u);

    password_autofill_agent_->PreviewField(
        form_util::GetFieldRendererId(element), kAliceUsername16);
    EXPECT_EQ(kAliceUsername, element.SuggestedValue().Utf8());
    EXPECT_TRUE(element.IsPreviewed());

    password_autofill_agent_->ClearPreviewedForm();
    EXPECT_TRUE(element.SuggestedValue().IsEmpty());
    EXPECT_TRUE(element.IsAutofilled());
    // The selection must stay intact.
    EXPECT_EQ(0u, element.SelectionStart());
    EXPECT_EQ(0u, element.SelectionEnd());
  }
}

// Tests that `ClearPreview` properly clears previewed username and password
// with username and password being previously autofilled.
TEST_F(PasswordAutofillAgentTest,
       ClearPreviewWithAutofilledUsernameAndPassword) {
  ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
  ResetFieldState(&password_element_, "sec", WebAutofillState::kPreviewed);

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, "sec", true);

  for (const auto& selected_element : {username_element_, password_element_}) {
    ASSERT_TRUE(
        SimulateElementClick(selected_element.GetAttribute("id").Ascii()));
    password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername16, kAlicePassword16);
    password_autofill_agent_->ClearPreviewedForm();

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState("ali", true, "sec", true);
    CheckUsernameSelection(3, 3);
  }
}

// Test that preview is cleared before the suggestion is filled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewBeforeFillingSuggestion) {
  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsDOMState(/*username=*/"", /*username_autofilled=*/false,
                          /*password=*/"", /*password_autofilled=*/false);
  for (const auto& selected_element : {username_element_, password_element_}) {
    SetFocused(selected_element);

    password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername16, kAlicePassword16);
    CheckTextFieldsSuggestedState(
        /*username=*/kAliceUsername, /*username_autofilled=*/true,
        /*password=*/kAlicePassword, /*password_autofilled=*/true);
    CheckTextFieldsDOMState(/*username=*/"", /*username_autofilled=*/true,
                            /*password=*/"", /*password_autofilled=*/true);
    EXPECT_TRUE(username_element_.IsPreviewed());
    EXPECT_TRUE(password_element_.IsPreviewed());

    password_autofill_agent_->FillPasswordSuggestion(kBobUsername16,
                                                     kBobPassword16);
    CheckTextFieldsSuggestedState(
        /*username=*/"", /*username_autofilled=*/true, /*password=*/"",
        /*password_autofilled=*/true);
    CheckTextFieldsDOMState(
        /*username=*/kBobUsername, /*username_autofilled=*/true,
        /*password=*/kBobPassword, /*password_autofilled=*/true);
    EXPECT_TRUE(username_element_.IsAutofilled());
    EXPECT_TRUE(password_element_.IsAutofilled());

    password_autofill_agent_->ClearPreviewedForm();
    CheckTextFieldsSuggestedState(
        /*username=*/"", /*username_autofilled=*/true, /*password=*/"",
        /*password_autofilled=*/true);
    CheckTextFieldsDOMState(
        /*username=*/kBobUsername, /*username_autofilled=*/true,
        /*password=*/kBobPassword, /*password_autofilled=*/true);
    EXPECT_TRUE(username_element_.IsAutofilled());
    EXPECT_TRUE(password_element_.IsAutofilled());

    ClearUsernameAndPasswordFieldValues();
  }
}

#if BUILDFLAG(IS_ANDROID)
// Tests that TryToShowKeyboardReplacingSurface() works correctly for fillable
// and non-fillable fields.
TEST_F(PasswordAutofillAgentTest, TryToShowKeyboardReplacingSurfaceUsername) {
  // Initially no fill data is available.
  WebInputElement random_element = GetInputElementByID("random_field");
  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      username_element_));
  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      random_element));
  EXPECT_FALSE(password_autofill_agent_->ShouldSuppressKeyboard());

  // This changes once fill data is simulated. `random_element` continue  to
  // have no fill data, though.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      username_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kNotFilled, username_element_.GetAutofillState());
  EXPECT_EQ(WebAutofillState::kNotFilled, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(
                  autofill::mojom::SubmissionReadinessState::kEmptyFields,
                  /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, TryToShowKeyboardReplacingSurfacePassword) {
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kNotFilled, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(
                  autofill::mojom::SubmissionReadinessState::kEmptyFields,
                  /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest,
       TryToShowKeyboardReplacingSurfaceWithWebauthnField) {
  LoadHTML(kFormWithUsernameFieldWebauthnHTML);
  UpdateUrlForHTML(kFormWithUsernameFieldWebauthnHTML);
  UpdateUsernameAndPasswordElements();
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(_, /*is_webauthn=*/true));
  base::RunLoop().RunUntilIdle();

  LoadHTML(kFormWithPasswordFieldWebauthnHTML);
  UpdateUrlForHTML(kFormWithPasswordFieldWebauthnHTML);
  UpdateUsernameAndPasswordElements();
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(_, /*is_webauthn=*/true));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest,
       TryToShowKeyboardReplacingSurfaceButDontEnableSubmission) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateUrlForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();
  // Enable filling for the old password field.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kNotFilled, password_element_.GetAutofillState());

  // As there are other input fields, don't enable automatic submission.
  EXPECT_CALL(
      fake_driver_,
      ShowKeyboardReplacingSurface(
          autofill::mojom::SubmissionReadinessState::kFieldAfterPasswordField,
          /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, KeyboardReplacingSurfaceSuppressesPopups) {
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoice(username_element_);
  EXPECT_CALL(fake_driver_, ShowKeyboardReplacingSurface);
  CheckSuggestionsNotShown();
}

TEST_F(PasswordAutofillAgentTest, KeyboardReplacingSurfaceClosed) {
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Touch to fill will be shown multiple times until
  // KeyboardReplacingSurfaceClosed() gets called.
  SimulateElementClick(password_element_);
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kNotFilled, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_, ShowKeyboardReplacingSurface);
  base::RunLoop().RunUntilIdle();

  password_autofill_agent_->KeyboardReplacingSurfaceClosed(true);
  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
  EXPECT_FALSE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kNotFilled, password_element_.GetAutofillState());

  // Reload the page and simulate fill.
  LoadHTML(kFormHTML);
  UpdateUrlForHTML(kFormHTML);
  UpdateUsernameAndPasswordElements();
  SimulateOnFillPasswordForm(fill_data_);
  SimulateElementClick(password_element_);

  // After the reload touch to fill is shown again.
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kNotFilled, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_, ShowKeyboardReplacingSurface);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, SubmissionReadiness_NoUsernameField) {
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateUrlForHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(
                  autofill::mojom::SubmissionReadinessState::kNoUsernameField,
                  /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, SubmissionReadiness_FieldsInBetween) {
  // Parse the "random_field" as username. Then, the "username" field should
  // block a submission.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF16(u"random_field"));
  ASSERT_TRUE(element);
  username_element_ = element.To<WebInputElement>();
  UpdateRendererIDsInFillData();

  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));

  EXPECT_CALL(fake_driver_, ShowKeyboardReplacingSurface(
                                autofill::mojom::SubmissionReadinessState::
                                    kFieldBetweenUsernameAndPassword,
                                /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, SubmissionReadiness_FieldAfterPassword) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateUrlForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));

  EXPECT_CALL(
      fake_driver_,
      ShowKeyboardReplacingSurface(
          autofill::mojom::SubmissionReadinessState::kFieldAfterPasswordField,
          /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, SubmissionReadiness_EmptyFields) {
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(
                  autofill::mojom::SubmissionReadinessState::kEmptyFields,
                  /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, SubmissionReadiness_MoreThanTwoFields) {
  SimulateOnFillPasswordForm(fill_data_);
  WebInputElement surname_element = GetInputElementByID("random_field");
  ASSERT_TRUE(surname_element);
  SimulateUserInputChangeForElement(surname_element, "Smith");

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(
                  autofill::mojom::SubmissionReadinessState::kMoreThanTwoFields,
                  /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, SubmissionReadiness_TwoFields) {
  LoadHTML(kSimpleWebpage);
  UpdateUrlForHTML(kSimpleWebpage);
  UpdateUsernameAndPasswordElements();

  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(
                  autofill::mojom::SubmissionReadinessState::kTwoFields,
                  /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, SubmissionReadiness_NoPasswordField) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateUrlForHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      username_element_));

  EXPECT_CALL(fake_driver_,
              ShowKeyboardReplacingSurface(
                  autofill::mojom::SubmissionReadinessState::kNoPasswordField,
                  /*is_webauthn=*/false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest,
       DontTryToShowKeyboardReplacingSurfaceOnReadonlyForm) {
  SetElementReadOnly(username_element_, true);
  SetElementReadOnly(password_element_, true);
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      username_element_));
  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
}

class PasswordAutofillAgentTestReadonlyElementVariationTest
    : public PasswordAutofillAgentTest,
      public testing::WithParamInterface<bool> {};

TEST_P(PasswordAutofillAgentTestReadonlyElementVariationTest,
       DontTryToShowKeyboardReplacingSurfaceOnReadonlyElement) {
  bool is_password_readonly = GetParam();

  WebInputElement readonly_element =
      is_password_readonly ? password_element_ : username_element_;
  WebInputElement editable_element =
      is_password_readonly ? username_element_ : password_element_;
  SetElementReadOnly(readonly_element, true);
  SetElementReadOnly(editable_element, false);
  base::RunLoop().RunUntilIdle();

  SimulateOnFillPasswordForm(fill_data_);

  // Firstly, check that Touch-To-Fill is not shown on the readonly element.
  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      readonly_element));

  // Secondly, check that Touch-To-Fill can be shown on the editable element.
  EXPECT_TRUE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      editable_element));
}

INSTANTIATE_TEST_SUITE_P(ReadonlyElementVariation,
                         PasswordAutofillAgentTestReadonlyElementVariationTest,
                         testing::Bool());

// Credentials are sent to the renderer even for sign-up forms as these may be
// eligible for filling via manual fall back. In this case, the username_field
// and password_field are not set. This test verifies that no failures are
// recorded in PasswordManager.FirstRendererFillingResult.
TEST_F(PasswordAutofillAgentTest,
       DontTryToShowKeyboardReplacingSurfaceSignUpForm) {
  LoadHTML(kSignupFormHTML);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("random_info"));
  ASSERT_TRUE(element);
  username_element_ = element.To<WebInputElement>();

  fill_data_.username_element_renderer_id = autofill::FieldRendererId();
  fill_data_.password_element_renderer_id = autofill::FieldRendererId();

  WebFormElement form_element =
      document.GetElementById("LoginTestForm").To<WebFormElement>();
  fill_data_.form_renderer_id = form_util::GetFormRendererId(form_element);

  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_FALSE(password_autofill_agent_->TryToShowKeyboardReplacingSurface(
      password_element_));
}
#endif  // BUILDFLAG(IS_ANDROID)

// Tests that `FillIntoFocusedField` doesn't fill read-only text fields.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedReadonlyTextField) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // If the field is readonly, it should not be affected.
  SetElementReadOnly(username_element_, true);
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/false, kAliceUsername16);
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);
}

// Tests that `FillIntoFocusedField` properly fills user-provided credentials.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedWritableTextField) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // The same field should be filled if it is writable.
  FocusElement(kUsernameName);
  SetElementReadOnly(username_element_, false);

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/false, kAliceUsername16);
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), false);
  CheckUsernameSelection(strlen(kAliceUsername), strlen(kAliceUsername));
}

// Tests that `FillIntoFocusedField` doesn't fill passwords in user fields.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedFieldOnlyIntoPasswordFields) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // Filling a password into a username field doesn't work.
  FocusElement(kUsernameName);

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/true, kAlicePassword16);
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // When a password field is focus, the filling works.
  FocusElement(kPasswordName);

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/true, kAlicePassword16);
  CheckTextFieldsDOMState(std::string(), false, kAlicePassword, true);
}

// Tests that `FillIntoFocusedField` fills last focused, not last clicked field.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedFieldForNonClickFocus) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // Click the username but shift the focus without click to the password.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  FocusElement(kPasswordName);
  // The completion should now affect ONLY the password field. Don't fill a
  // password so the error on failure shows where the filling happened.
  // (see FillIntoFocusedFieldOnlyIntoPasswordFields).

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/false, u"TextToFill");
  CheckTextFieldsDOMState(std::string(), false, "TextToFill", true);
}

// Tests that `FillInfoField` doesn't fill read-only text fields.
TEST_F(PasswordAutofillAgentTest, FillIntoReadonlyTextField) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);

  // If the field is readonly, it should not be affected.
  SetElementReadOnly(username_element_, true);
  password_autofill_agent_->FillField(
      form_util::GetFieldRendererId(username_element_), kAliceUsername16);
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);
}

// Tests that `FillInfoField` correctly fills the username field.
TEST_F(PasswordAutofillAgentTest, FillIntoUsernameField) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);

  password_autofill_agent_->FillField(
      form_util::GetFieldRendererId(username_element_), kAliceUsername16);
  CheckTextFieldsDOMState(
      /*username=*/kAliceUsername, /*username_autofilled=*/true,
      /*password=*/std::string(), /*password_autofilled=*/false);
}

// Tests that `FillInfoField` correctly fills the password field.
TEST_F(PasswordAutofillAgentTest, FillIntoPasswordField) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);

  password_autofill_agent_->FillField(
      form_util::GetFieldRendererId(password_element_), kAlicePassword16);
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/kAlicePassword, /*password_autofilled=*/true);
}

// Tests that `FillInfoField` can fill into a random field.
TEST_F(PasswordAutofillAgentTest, FillIntoRandomField) {
  WebInputElement random_element = GetInputElementByID("random_field");

  // The field should not be autocompleted.
  EXPECT_EQ(std::string(), random_element.Value().Utf8());

  password_autofill_agent_->FillField(
      form_util::GetFieldRendererId(random_element), kAliceUsername16);
  EXPECT_EQ(kAliceUsername, random_element.Value().Utf8());
}

// Tests that `FillInfoField` doesn't fill non-existent fields.
TEST_F(PasswordAutofillAgentTest, FillIntoNonExistingField) {
  WebInputElement random_element = GetInputElementByID("random_field");

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);
  EXPECT_EQ(std::string(), random_element.Value().Utf8());

  password_autofill_agent_->FillField(FieldRendererId(), kAliceUsername16);
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(
      /*username=*/std::string(), /*username_autofilled=*/false,
      /*password=*/std::string(), /*password_autofilled=*/false);
  EXPECT_EQ(std::string(), random_element.Value().Utf8());
}

// Tests that `ClearPreview` properly clears previewed username and password
// with neither username nor password being previously autofilled.
TEST_F(PasswordAutofillAgentTest,
       ClearPreviewWithNotAutofilledUsernameAndPassword) {
  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  for (const auto& selected_element : {username_element_, password_element_}) {
    ASSERT_TRUE(
        SimulateElementClick(selected_element.GetAttribute("id").Ascii()));
    password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername16, kAlicePassword16);
    password_autofill_agent_->ClearPreviewedForm();

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);
    CheckUsernameSelection(0, 0);
  }
}

// Tests that logging is off by default.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_NoMessage) {
  SendVisiblePasswordForms();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_record_save_progress());
}

// Test that logging can be turned on by a message.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_Activated) {
  // Turn the logging on.
  password_autofill_agent_->SetLoggingState(true);

  SendVisiblePasswordForms();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_record_save_progress());
}

// Test that logging can be turned off by a message.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_Deactivated) {
  // Turn the logging on and then off.
  password_autofill_agent_->SetLoggingState(true);
  password_autofill_agent_->SetLoggingState(false);

  SendVisiblePasswordForms();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_record_save_progress());
}

// Tests that one user click on a username field is sufficient to bring up a
// credential suggestion popup, and the user can autocomplete the password by
// selecting the credential from the popup.
TEST_F(PasswordAutofillAgentTest, ClickAndSelect) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  // SimulateElementClick() is called so that a user gesture is actually made
  // and the password can be filled. However, SimulateElementClick() does not
  // actually lead to the AutofillAgent's InputElementClicked() method being
  // called, so SimulateSuggestionChoice has to manually call
  // InputElementClicked().
  ClearUsernameAndPasswordFieldValues();
  SimulateOnFillPasswordForm(fill_data_);
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.SuggestionPopupTriggerSource",
      static_cast<int>(autofill::AutofillSuggestionTriggerSource::
                           kFormControlElementClicked),
      1);
  SimulateSuggestionChoice(username_element_);
  CheckSuggestions(kAliceUsername16, true);

  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that password suggestions are prefix matched against typed username.
// TODO(b:322923603): Clean up when the feature is launched.
TEST_F(PasswordAutofillAgentTest, SuggestionsPrefixMatchedByTypedUsername) {
  ConfigurePasswordSuggestionFiltering(/*enabled=*/false);
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  ClearUsernameAndPasswordFieldValues();
  // Make sure there's password data to fill in the field.
  SimulateOnFillPasswordForm(fill_data_);
  // Enter the value manually.
  SimulateUsernameTyping("ali");

  // Simulate a user clicking on the username element. This should produce a
  // message with all the usernames.
  SimulateElementClick(username_element_);
  CheckSuggestions(u"ali", /*show_all=*/false);
  base::RunLoop().RunUntilIdle();
}

// Tests that all password suggestiona are shown when suggestion filtering is
// disabled.
TEST_F(PasswordAutofillAgentTest,
       SuggestionsNotPrefixMatchedWhenFeatureEnabled) {
  ConfigurePasswordSuggestionFiltering(/*enabled=*/true);
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  ClearUsernameAndPasswordFieldValues();
  // Make sure there's password data to fill in the field.
  SimulateOnFillPasswordForm(fill_data_);
  // Enter the value manually.
  SimulateUsernameTyping("ali");

  // Simulate a user clicking on the username element. This should produce a
  // message with all the usernames.
  SimulateElementClick(username_element_);
  CheckSuggestions(u"ali", /*show_all=*/true);
  base::RunLoop().RunUntilIdle();
}

// Tests that the popup is suppressed when the user selects address or payments
// fallback even when the triggering field that is classified as password.
TEST_F(PasswordAutofillAgentTest,
       NoPopupOnPasswordFieldWhereAddressOrPaymentsManualFallbackWasSelected) {
  SimulateOnFillPasswordForm(fill_data_);
  // This call is necessary to setup the autofill agent appropriate for the
  // user selection; simulates the menu actually popping up.
  SimulatePointClick(gfx::Point(1, 1));

  // No popup request when using address/payment/plus address manual fallback.
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
  autofill_agent_->TriggerSuggestions(
      form_util::GetFieldRendererId(username_element_),
      AutofillSuggestionTriggerSource::kManualFallbackAddress);
  autofill_agent_->TriggerSuggestions(
      form_util::GetFieldRendererId(username_element_),
      AutofillSuggestionTriggerSource::kManualFallbackPayments);
  autofill_agent_->TriggerSuggestions(
      form_util::GetFieldRendererId(username_element_),
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses);

  // However, the popup is requested for password manual fallback.
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  autofill_agent_->TriggerSuggestions(
      form_util::GetFieldRendererId(username_element_),
      AutofillSuggestionTriggerSource::kManualFallbackPasswords);
}

TEST_F(PasswordAutofillAgentTest,
       NoPopupOnPasswordFieldWithoutSuggestionsByDefault) {
  ClearUsernameAndPasswordFieldValues();
  UpdateRendererIDsInFillData();

  // A call to InformNoSavedCredentials(should_show_popup_without_passwords) is
  // what informs the agent whether it should show the popup even without
  // suggestions. In this test, that call hasn't happened yet, so the popup
  // should NOT show up without suggestions.
  ASSERT_TRUE(SimulateElementClick(kPasswordName));

  CheckSuggestionsNotShown();
}

// With butter, passwords fields should always trigger the popup so the user can
// unlock account-stored suggestions from there.
TEST_F(PasswordAutofillAgentTest, ShowPopupOnPasswordFieldWithoutSuggestions) {
  ClearUsernameAndPasswordFieldValues();
  UpdateRendererIDsInFillData();

  // InformNoSavedCredentials(should_show_popup_without_passwords) tells the
  // agent to show the popup even without suggestions.
  password_autofill_agent_->InformNoSavedCredentials(
      /*should_show_popup_without_passwords=*/true);

  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions)
      .Times(NumShowSuggestionsCalls());
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  base::RunLoop().RunUntilIdle();
}

// Before butter, passwords fields should never trigger the popup on password
// passwords fields without suggestions since it would not be helpful.
TEST_F(PasswordAutofillAgentTest, NoPopupOnPasswordFieldWithoutSuggestions) {
  ClearUsernameAndPasswordFieldValues();
  UpdateRendererIDsInFillData();

  // InformNoSavedCredentials(should_show_popup_without_passwords) tells the
  // agent NOT to show the popup without suggestions.
  password_autofill_agent_->InformNoSavedCredentials(
      /*should_show_popup_without_passwords=*/false);

  ASSERT_TRUE(SimulateElementClick(kPasswordName));

  CheckSuggestionsNotShown();
}

// Tests the autosuggestions that are given when the element is clicked.
// Specifically, tests when the user clicks on the username element after page
// load and the element is autofilled, when the user clicks on an element that
// has a matching username.
TEST_F(PasswordAutofillAgentTest, CredentialsOnClick) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFieldValues();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions)
      .Times(NumShowSuggestionsCalls());
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the username element. This should produce a
  // message with all the usernames.
  SimulateElementClick(username_element_);
  CheckSuggestions(std::u16string(), true);

  // Now simulate a user typing in a saved username. The list is filtered.
  EXPECT_CALL(fake_driver_,
              ShowPasswordSuggestions(
                  Field(&autofill::PasswordSuggestionRequest::element_id,
                        form_util::GetFieldRendererId(username_element_))))
      .Times(NumShowSuggestionsCalls());
  SimulateUsernameTyping(kAliceUsername);
}

// Tests that there is an autosuggestion from the password manager when the
// user clicks on the password field.
TEST_F(PasswordAutofillAgentTest, NoCredentialsOnPasswordClick) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFieldValues();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the password element. This should produce no
  // message.
  SimulateElementClick(password_element_);
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions)
      .Times(NumShowSuggestionsCalls());
  base::RunLoop().RunUntilIdle();
}

// The user types in a username and a password, but then just before sending
// the form off, a script clears them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_ScriptCleared) {
  LoadHTML(kSignupFormHTML);
  WebInputElement username_element = GetInputElementByID("random_info");
  ASSERT_TRUE(username_element);
  SimulateUserInputChangeForElement(username_element, "username");

  WebInputElement new_password_element = GetInputElementByID("new_password");
  ASSERT_TRUE(new_password_element);
  SimulateUserInputChangeForElement(new_password_element, "random");

  WebInputElement confirmation_password_element =
      GetInputElementByID("confirm_password");
  ASSERT_TRUE(confirmation_password_element);
  SimulateUserInputChangeForElement(confirmation_password_element, "random");

  // Simulate that the username and the password values were cleared by the
  // site's JavaScript before submit.
  username_element.SetValue(WebString());
  new_password_element.SetValue(WebString());
  confirmation_password_element.SetValue(WebString());

  // Submit form.
  FormTracker& tracker = test_api(*autofill_agent_).form_tracker();
  static_cast<content::RenderFrameObserver&>(tracker).WillSubmitForm(
      username_element.Form());

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      form_util::GetFormRendererId(username_element.Form()), u"username", u"",
      u"random");
  fake_driver_.form_data_submitted();
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but this time
// it's the user who clears the username and the password. This test checks
// that in that case, the last non-empty username and password are not
// remembered.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_UserCleared) {
  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the user actually cleared the username and password again.
  SimulateUsernameTyping("");
  SimulatePasswordTyping("");

  SubmitForm();

  // Observe that the PasswordAutofillAgent respects the user having cleared the
  // password.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), u"", u"", u"");
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but uses the
// new password instead of the current password.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_New) {
  const char kNewPasswordFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username' autocomplete='username'/>"
      "  <INPUT type='password' id='password' autocomplete='new-password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kNewPasswordFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the username and the password value was cleared by
  // the site's JavaScript before submit.
  username_element_.SetValue(WebString());
  password_element_.SetValue(WebString());

  SubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      form_util::GetFormRendererId(username_element_.Form()), u"temp", u"",
      u"random");
}

// Similar to RememberLastNonEmptyUsernameAndPasswordOnSubmit_New, but uses
// no password fields on single username form
TEST_F(PasswordAutofillAgentTest, RememberLastNonEmptySingleUsername) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();

  SimulateUsernameTyping("temp");

  SubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      form_util::GetFormRendererId(username_element_.Form()), u"temp", u"",
      u"");
}

// The user first accepts a suggestion, but then overwrites the password. This
// test checks that the overwritten password is not reverted back.
TEST_F(PasswordAutofillAgentTest,
       NoopEditingDoesNotOverwriteManuallyEditedPassword) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  fill_data_.wait_for_username = true;
  SimulateUsernameTyping(kAliceUsername);
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoice(username_element_);
  const std::string old_username(username_element_.Value().Utf8());
  const std::string old_password(password_element_.Value().Utf8());
  const std::string new_password(old_password + "modify");

  // The user changes the password.
  SimulatePasswordTyping(new_password);

  // Change focus in between to make sure blur events don't trigger filling.
  SetFocused(password_element_);
  SetFocused(username_element_);

  // The password should have stayed as the user changed it.
  // The username should not be autofilled, because it was typed by the user.
  CheckTextFieldsDOMState(old_username, false, new_password, false);
  // The password should not have a suggested value.
  CheckUsernameDOMStatePasswordSuggestedState(old_username, false,
                                              std::string(), false);
}

// The user types the username, then accepts a suggestion. This test checks
// that autofilling does not rewrite the username, if the value is already
// there.
TEST_F(PasswordAutofillAgentTest, AcceptingSuggestionDoesntRewriteUsername) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  fill_data_.wait_for_username = true;
  SimulateUsernameTyping(kAliceUsername);
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoice(username_element_);

  const std::string username(username_element_.Value().Utf8());
  const std::string password(password_element_.Value().Utf8());

  // The password was autofilled. The username was not.
  CheckTextFieldsDOMState(username, false, password, true);
}

// The user types in a username and a password, but then just before sending
// the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       RememberLastTypedUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), u"temp", u"random", u"");
}

TEST_F(PasswordAutofillAgentTest, RememberFieldPropertiesOnSubmit) {
  FocusElement("random_field");
  SimulateUsernameTyping("typed_username");
  SimulatePasswordTyping("typed_password");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  std::map<std::u16string, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[u"random_field"] = FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[u"username"] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[u"password"] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;

  ExpectFieldPropertiesMasks(PasswordFormSubmitted, expected_properties_masks,
                             SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
}

TEST_F(PasswordAutofillAgentTest, FixEmptyFieldPropertiesOnSubmit) {
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate a user click so that the password field's real value is filled.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));

  // Simulate replacing the username and password field.
  static constexpr char kJavaScript[] =
      "const old_username = document.getElementById('username');"
      "const old_password = document.getElementById('password');"

      "const new_username = document.createElement('input');"
      "new_username.value = old_username.value;"
      "new_username.id = 'new_username';"

      "const new_password = document.createElement('input');"
      "new_password.value = old_password.value;"
      "new_password.id = 'new_password';"

      "const form = document.getElementById('LoginTestForm');"
      "form.appendChild(new_username);"
      "form.appendChild(new_password);"
      "form.removeChild(old_username);"
      "form.removeChild(old_password);";

  ExecuteJavaScriptForTests(kJavaScript);
  auto form_element = GetMainFrame()
                          ->GetDocument()
                          .GetElementById(WebString::FromUTF8("LoginTestForm"))
                          .To<WebFormElement>();
  SaveAndSubmitForm(form_element);

  std::map<std::u16string, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[u"new_username"] =
      FieldPropertiesFlags::kAutofilledOnPageLoad;
  expected_properties_masks[u"new_password"] =
      FieldPropertiesFlags::kAutofilledOnPageLoad;

  ExpectFieldPropertiesMasks(PasswordFormSubmitted, expected_properties_masks,
                             SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
}

TEST_F(PasswordAutofillAgentTest,
       RememberFieldPropertiesOnSameDocumentNavigation) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  FireAjaxSucceeded();

  std::map<std::u16string, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[u"username"] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[u"password"] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;

  ExpectFieldPropertiesMasks(PasswordFormSameDocumentNavigation,
                             expected_properties_masks,
                             SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

TEST_F(PasswordAutofillAgentTest,
       RememberFieldPropertiesOnSameDocumentNavigation_2) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());
  ForceLayoutUpdate();

  base::RunLoop().RunUntilIdle();

  std::map<std::u16string, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[u"username"] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[u"password"] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;

  ExpectFieldPropertiesMasks(PasswordFormSameDocumentNavigation,
                             expected_properties_masks,
                             SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

// The username/password is autofilled by password manager then just before
// sending the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still get the username and the password autofilled.
TEST_F(PasswordAutofillAgentTest,
       RememberLastAutofilledUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the autofilled
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kAliceUsername16,
      kAlicePassword16, u"");
}

// The username/password is autofilled by password manager then user types in a
// username and a password. Then just before sending the form off, a script
// changes them. This test checks that PasswordAutofillAgent can still remember
// the username and the password typed by the user.
TEST_F(
    PasswordAutofillAgentTest,
    RememberLastTypedAfterAutofilledUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateOnFillPasswordForm(fill_data_);

  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), u"temp", u"random", u"");
}

// The user starts typing username then it is autofilled.
// PasswordAutofillAgent should remember the username that was autofilled,
// not last typed.
TEST_F(PasswordAutofillAgentTest, RememberAutofilledUsername) {
  SimulateUsernameTyping("Te");
  // Simulate that the username was changed by autofilling.
  username_element_.SetValue(WebString("temp"));
  SimulatePasswordTyping("random");

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), u"temp", u"random", u"");
}

// The user starts typing username then javascript suggests to select another
// username that was generated based on typed field value (e.g. surname field).
// PasswordAutofillAgent should remember the username that was selected,
// not last typed.
TEST_F(PasswordAutofillAgentTest,
       RememberUsernameGeneratedBasingOnTypedFields) {
  SimulateUsernameTyping("Temp");
  SimulatePasswordTyping("random");

  // Suppose that "random_field" contains surname.
  WebInputElement surname_element = GetInputElementByID("random_field");
  SimulateUserInputChangeForElement(surname_element, "Smith");

  // Simulate that the user selected username that was generated by script.
  username_element_.SetValue(WebString("foo.smith"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), u"foo.smith", u"random", u"");
}

// If credentials contain username+password but the form contains only a
// password field, we don't autofill on page load.
TEST_F(PasswordAutofillAgentTest, DontFillFormWithNoUsername) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();

  SimulateOnFillPasswordForm(fill_data_);

  // As the credential contains a username, but the form does not, the
  // credential is not filled.
  CheckFirstFillingResult(FillingResult::kFoundNoPasswordForUsername);
}

// Tests the standard behavior of the filling a suggestions by passing the IDs
// of the elements to be filled along with the values to fill.
TEST_F(PasswordAutofillAgentTest, FillPasswordSuggestionById) {
  ASSERT_TRUE(SimulateElementClick(kUsernameName));

  password_autofill_agent_->PreviewPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), kAliceUsername16,
      kAlicePassword16);

  EXPECT_EQ(username_element_.SuggestedValue().Utf16(), kAliceUsername16);
  EXPECT_TRUE(username_element_.IsPreviewed());
  EXPECT_EQ(password_element_.SuggestedValue().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.IsPreviewed());

  password_autofill_agent_->FillPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), kAliceUsername16,
      kAlicePassword16);

  EXPECT_EQ(username_element_.SelectionStart(), 5u);
  EXPECT_EQ(username_element_.SelectionEnd(), 5u);
  EXPECT_EQ(username_element_.Value().Utf16(), kAliceUsername16);
  EXPECT_TRUE(username_element_.IsAutofilled());
  EXPECT_EQ(password_element_.Value().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.IsAutofilled());
}

// Tests the behavior of FillPasswordSuggestionById when the elements to be
// filled are read-only.
TEST_F(PasswordAutofillAgentTest, FillPasswordSuggestionById_ReadOnlyElements) {
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  SetElementReadOnly(username_element_, true);
  SetElementReadOnly(password_element_, true);

  password_autofill_agent_->PreviewPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), kAliceUsername16,
      kAlicePassword16);

  EXPECT_EQ(username_element_.SuggestedValue().Utf16(), u"");
  EXPECT_FALSE(username_element_.IsPreviewed());
  EXPECT_EQ(password_element_.SuggestedValue().Utf16(), u"");
  EXPECT_FALSE(password_element_.IsPreviewed());

  password_autofill_agent_->FillPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), kAliceUsername16,
      kAlicePassword16);

  EXPECT_EQ(username_element_.Value().Utf16(), u"");
  EXPECT_FALSE(username_element_.IsAutofilled());
  EXPECT_EQ(password_element_.Value().Utf16(), u"");
  EXPECT_FALSE(password_element_.IsAutofilled());
}

// Tests the behavior of FillPasswordSuggestionById when no username value is
// present in the passed credentials.
TEST_F(PasswordAutofillAgentTest, FillPasswordSuggestionById_NoUsernameValue) {
  username_element_.SetValue(WebString(kBobUsername16));
  ASSERT_TRUE(SimulateElementClick(kPasswordName));

  password_autofill_agent_->PreviewPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), u"", kAlicePassword16);

  EXPECT_EQ(username_element_.SuggestedValue().Utf16(), u"");
  EXPECT_FALSE(username_element_.IsPreviewed());
  EXPECT_EQ(password_element_.SuggestedValue().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.IsPreviewed());

  password_autofill_agent_->FillPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), u"", kAlicePassword16);

  EXPECT_EQ(username_element_.Value().Utf16(), kBobUsername16);
  EXPECT_FALSE(username_element_.IsAutofilled());
  EXPECT_EQ(password_element_.Value().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.IsAutofilled());
}

// Tests the behavior of FillPasswordSuggestionById when the form contains no
// username element.
TEST_F(PasswordAutofillAgentTest, FillPasswordSuggestionById_NoUsername) {
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  password_element_ = GetInputElementByID(kPasswordName);
  ASSERT_TRUE(SimulateElementClick(kPasswordName));

  password_autofill_agent_->PreviewPasswordSuggestionById(
      FieldRendererId(), form_util::GetFieldRendererId(password_element_),
      kAliceUsername16, kAlicePassword16);

  EXPECT_EQ(password_element_.SuggestedValue().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.Value().IsEmpty());
  EXPECT_TRUE(password_element_.IsPreviewed());

  password_autofill_agent_->FillPasswordSuggestionById(
      FieldRendererId(), form_util::GetFieldRendererId(password_element_),
      kAliceUsername16, kAlicePassword16);

  EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
  EXPECT_EQ(password_element_.Value().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.IsAutofilled());
}

// Tests the behavior of FillPasswordSuggestionById when the form contains no
// password element.
TEST_F(PasswordAutofillAgentTest, FillPasswordSuggestionById_NoPassword) {
  LoadHTML(kSingleUsernameFormHTML);
  username_element_ = GetInputElementByID(kUsernameName);
  ASSERT_TRUE(SimulateElementClick(kUsernameName));

  password_autofill_agent_->PreviewPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_), FieldRendererId(),
      kAliceUsername16, kAlicePassword16);

  EXPECT_EQ(username_element_.SuggestedValue().Utf16(), kAliceUsername16);
  EXPECT_TRUE(username_element_.Value().IsEmpty());
  EXPECT_TRUE(username_element_.IsPreviewed());

  password_autofill_agent_->FillPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_), FieldRendererId(),
      kAliceUsername16, kAlicePassword16);

  EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
  EXPECT_EQ(username_element_.Value().Utf16(), kAliceUsername16);
  EXPECT_TRUE(username_element_.IsAutofilled());
}

// Tests the behavior of FillPasswordSuggestionById when neither of the passed
// elements are focused.
TEST_F(PasswordAutofillAgentTest, FillPasswordSuggestionById_NoFocusedElement) {
  ASSERT_TRUE(SimulateElementClick("random_field"));

  password_autofill_agent_->PreviewPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), kAliceUsername16,
      kAlicePassword16);

  EXPECT_EQ(username_element_.SuggestedValue().Utf16(), kAliceUsername16);
  EXPECT_TRUE(username_element_.IsPreviewed());
  EXPECT_EQ(password_element_.SuggestedValue().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.IsPreviewed());

  password_autofill_agent_->FillPasswordSuggestionById(
      form_util::GetFieldRendererId(username_element_),
      form_util::GetFieldRendererId(password_element_), kAliceUsername16,
      kAlicePassword16);

  EXPECT_EQ(username_element_.Value().Utf16(), kAliceUsername16);
  EXPECT_TRUE(username_element_.IsAutofilled());
  EXPECT_EQ(password_element_.Value().Utf16(), kAlicePassword16);
  EXPECT_TRUE(password_element_.IsAutofilled());
}

TEST_F(PasswordAutofillAgentTest, ShowPopupOnEmptyPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateUrlForHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();
  fill_data_.preferred_login.username_value.clear();
  fill_data_.additional_logins.clear();

  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kPasswordName);

  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Show popup suggestion when the password field is empty.
  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, std::u16string(), kAlicePassword16);
  CheckSuggestions(std::u16string(), true);
  EXPECT_EQ(kAlicePassword16, password_element_.Value().Utf16());
  EXPECT_TRUE(password_element_.IsAutofilled());
}

TEST_F(PasswordAutofillAgentTest, ShowPopupOnAutofilledPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateUrlForHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();
  fill_data_.preferred_login.username_value.clear();
  fill_data_.additional_logins.clear();

  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kPasswordName);

  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Show popup suggestion when the password field is autofilled.
  password_element_.SetValue("123");
  password_element_.SetAutofillState(WebAutofillState::kAutofilled);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, std::u16string(), kAlicePassword16);
  CheckSuggestions(std::u16string(), true);
  EXPECT_EQ(kAlicePassword16, password_element_.Value().Utf16());
  EXPECT_TRUE(password_element_.IsAutofilled());
}

TEST_F(PasswordAutofillAgentTest, NotShowPopupPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateUrlForHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();
  fill_data_.preferred_login.username_value.clear();
  fill_data_.additional_logins.clear();

  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Do not show popup suggestion when the password field is not-empty and not
  // autofilled.
  password_element_.SetValue("123");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, std::u16string(), kAlicePassword16);
  CheckSuggestionsNotShown();
}

// Tests with fill-on-account-select enabled that if the username element is
// read-only and filled with an unknown username, then the password field is not
// highlighted as autofillable (regression test for https://crbug.com/442564).
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyReadonlyUnknownUsername) {
  ClearUsernameAndPasswordFieldValues();

  username_element_.SetValue("foobar");
  SetElementReadOnly(username_element_, true);

  CheckUsernameDOMStatePasswordSuggestedState(std::string("foobar"), false,
                                              std::string(), false);
}

// The user types in a username and a password. Then JavaScript changes password
// field to readonly state before submit. PasswordAutofillAgent can correctly
// process readonly password field. This test models behaviour of gmail.com.
TEST_F(PasswordAutofillAgentTest, ReadonlyPasswordFieldOnSubmit) {
  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that JavaScript makes password field readonly.
  SetElementReadOnly(password_element_, true);

  SubmitForm();

  // Observe that the PasswordAutofillAgent can correctly process submitted
  // form.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), u"temp", u"random", u"");
}

// Verify that typed passwords are saved correctly when autofill and generation
// both trigger. Regression test for https://crbug.com/493455
TEST_F(PasswordAutofillAgentTest, PasswordGenerationTriggered_TypedPassword) {
  SimulateOnFillPasswordForm(fill_data_);

  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"password", /*confirm_password_id=*/nullptr);

  // Generation event is triggered due to focus events.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus())
      .Times(testing::AnyNumber());
  SimulateUsernameTyping("NewGuy");
  SimulatePasswordTyping("NewPassword");

  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), u"NewGuy", u"NewPassword", u"");
}

// Verify that generated passwords are saved correctly when autofill and
// generation both trigger. Regression test for https://crbug.com/493455.
TEST_F(PasswordAutofillAgentTest,
       PasswordGenerationTriggered_GeneratedPassword) {
  SimulateOnFillPasswordForm(fill_data_);

  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"password", /*confirm_password_id=*/nullptr);
  // Simulate the user clicks on a password field, that leads to showing
  // generation pop-up. GeneratedPasswordAccepted can't be called without it.
  ASSERT_TRUE(SimulateElementClick(kPasswordName));

  std::u16string password = u"NewPass22";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);

  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kAliceUsername16, u"NewPass22",
      u"");
}

// TODO(crbug.com/40100455): Figure out whether this test is simulating a
// realistic sequence of events.
TEST_F(PasswordAutofillAgentTest,
       ResetPasswordGenerationWhenFieldIsAutofilled) {
  // A user generates password.
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"password", /*confirm_password_id=*/nullptr);
  // Simulate the user clicks on a password field, that leads to showing
  // generation pop-up. GeneratedPasswordAccepted can't be called without it.
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  std::u16string password = u"NewPass22";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);

  // The form should not be autofilled on the next call of FillPasswordForm
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated);
  SimulateOnFillPasswordForm(fill_data_);
  base::RunLoop().RunUntilIdle();

  // The password field shouldn't reveal the value on focusing.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("password"));
  ASSERT_TRUE(element);
  WebInputElement password_element = element.To<WebInputElement>();
  EXPECT_FALSE(password_element.ShouldRevealPassword());
  EXPECT_FALSE(password_element.IsAutofilled());

  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kAliceUsername16, u"NewPass22",
      u"");

  // Then user selects another account on Fill On Account Select
  SimulateSuggestionChoiceOfUsernameAndPassword(username_element_,
                                                kBobUsername16, kBobPassword16);
  base::RunLoop().RunUntilIdle();

  // The password field still shouldn't reveal the value on focusing.
  EXPECT_FALSE(password_element.ShouldRevealPassword());
  EXPECT_TRUE(password_element.IsAutofilled());

  test_api(*autofill_agent_).OnFormNoLongerSubmittable();
  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kBobUsername16, kBobPassword16,
      u"");
}

// If password generation is enabled for a field, password autofill should not
// show UI.
TEST_F(PasswordAutofillAgentTest, PasswordGenerationSupersedesAutofill) {
  LoadHTML(kSignupFormHTML);

  // Update password_element_;
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("new_password"));
  ASSERT_TRUE(element);
  password_element_ = element.To<WebInputElement>();

  // Update fill_data_ for the new form and simulate filling. Pretend as if
  // the password manager didn't detect a username field so it will try to
  // show UI when the password field is focused.
  fill_data_.wait_for_username = true;
  fill_data_.preferred_login.username_value.clear();
  fill_data_.username_element_renderer_id = FieldRendererId();
  UpdateUrlForHTML(kSignupFormHTML);
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate generation triggering.
  SetFoundFormEligibleForGeneration(password_generation_,
                                    GetMainFrame()->GetDocument(),
                                    /*new_password_id=*/"new_password",
                                    /*confirm_password_id=*/"confirm_password");

  // Simulate the field being clicked to start typing. This should trigger
  // generation but not password autofill.
  ASSERT_TRUE(SimulateElementClick("new_password"));
  // TODO(crbug.com/40279043): Expect the call precisely once.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable)
      .Times(NumShowSuggestionsCalls());
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
  CheckSuggestionsNotShown();

  // On destruction the state is updated.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus())
      .Times(testing::AnyNumber());
}

// Tests the following scenario: 1) user triggers manual generation, 2) user
// erases the generated password from the field, 3) password suggestions should
// be displayed when available after the field is focused again.
// Regression test for crbug/1495325.
TEST_F(PasswordAutofillAgentTest, CanShowSuggestionsAfterManualGeneration) {
  // Ensure TTF isn't shown when the user focuses the password field.
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kPasswordName);

  // Simulate receiving credentials for filling from the browser.
  SimulateOnFillPasswordForm(fill_data_);

  // Focus `password_element_` and verify that suggestions are shown to the
  // user.
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  CheckSuggestions(/*typed_username=*/u"", true);

  // Simulate manual generation triggering.
  base::test::TestFuture<const std::optional<
      ::autofill::password_generation::PasswordGenerationUIData>&>
      future_for_waiting;
  password_generation_->TriggeredGeneratePassword(
      future_for_waiting.GetCallback());
  EXPECT_TRUE(future_for_waiting.Wait());

  const std::u16string kPassword = u"NewPass24";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(kPassword)));
  password_generation_->GeneratedPasswordAccepted(kPassword);
  ASSERT_EQ(password_element_.Value().Utf16(), kPassword);

  // Clear the password field value.
  password_element_.SetValue(WebString());
  password_generation_->TextDidChangeInTextField(password_element_);

  // Focus the password element again and verify that suggestions are shown to
  // the user.
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  CheckSuggestions(/*typed_username=*/u"", true);
}

// Tests that a password change form is properly filled with the username and
// password.
TEST_F(PasswordAutofillAgentTest, FillSuggestionPasswordChangeForms) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateUrlForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();
  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    SimulateElementClick(selected_element);
    // Neither field should be autocompleted.
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);

    password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                     kAlicePassword16);
    CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

    ClearUsernameAndPasswordFieldValues();
  }
}

// Tests that one user click on a username field is sufficient to bring up a
// credential suggestion popup on a change password form.
TEST_F(PasswordAutofillAgentTest,
       SuggestionsOnUsernameFieldOfChangePasswordForm) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateUrlForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kPasswordName);

  ClearUsernameAndPasswordFieldValues();
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Simulate a user clicking on the username element. This should produce a
  // message.
  SimulateElementClick(username_element_);
  CheckSuggestions(u"", true);
}

// Tests that one user click on a password field is sufficient to bring up a
// credential suggestion popup on a change password form.
TEST_F(PasswordAutofillAgentTest,
       SuggestionsOnPasswordFieldOfChangePasswordForm) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateUrlForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kPasswordName);

  ClearUsernameAndPasswordFieldValues();
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Simulate a user clicking on the password element. This should produce a
  // message.
  SimulateElementClick(password_element_);
  CheckSuggestions(u"", true);
}

// Tests that only the password field is autocompleted when the browser sends
// back data with only one credentials and empty username.
TEST_F(PasswordAutofillAgentTest, NotAutofillNoUsername) {
  fill_data_.preferred_login.username_value.clear();
  fill_data_.username_element_renderer_id = autofill::FieldRendererId();
  fill_data_.additional_logins.clear();
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);
}

// Tests that the username field is not marked as autofilled when fill data has
// the empty username.
TEST_F(PasswordAutofillAgentTest,
       AutofillNoUsernameWhenOtherCredentialsStored) {
  fill_data_.preferred_login.username_value.clear();
  ASSERT_FALSE(fill_data_.additional_logins.empty());
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, NoForm_PromptForAJAXSubmitWithoutNavigation) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  FireAjaxSucceeded();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      FormRendererId(), u"Bob", u"mypassword", u"",
      SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

TEST_F(PasswordAutofillAgentTest,
       NoForm_PromptForAJAXSubmitWithoutNavigation_2) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());
  ForceLayoutUpdate();

  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      FormRendererId(), u"Bob", u"mypassword", u"",
      SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

// In this test, a <div> wrapping a form is hidden via display:none after an
// Ajax request. The test verifies that we offer to save the password, as hiding
// the <div> also hiding the <form>.
TEST_F(PasswordAutofillAgentTest, PromptForAJAXSubmitAfterHidingParentElement) {
  LoadHTML(kDivWrappedFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string hide_element =
      "var outerDiv = document.getElementById('outer');"
      "outerDiv.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_element.c_str());
  ForceLayoutUpdate();

  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      GetFormUniqueRendererId("form"), u"Bob", u"mypassword", u"",
      SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

// In this test, a <div> wrapping a form is removed from the DOM after an Ajax
// request. The test verifies that we offer to save the password, as removing
// the <div> also removes the <form>.
TEST_F(PasswordAutofillAgentTest,
       PromptForAJAXSubmitAfterDeletingParentElement) {
  LoadHTML(kDivWrappedFormHTML);
  UpdateUsernameAndPasswordElements();
  FormRendererId renderer_id = GetFormUniqueRendererId("form");

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string delete_element =
      "var outerDiv = document.getElementById('outer');"
      "var innerDiv = document.getElementById('inner');"
      "outerDiv.removeChild(innerDiv);";
  ExecuteJavaScriptForTests(delete_element.c_str());

  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      renderer_id, u"Bob", u"mypassword", u"",
      SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

TEST_F(PasswordAutofillAgentTest,
       NoForm_NoPromptForAJAXSubmitWithoutNavigationAndElementsVisible) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_password_form_submitted());
}

// Tests that no save prompt is shown when an unowned form is changed and AJAX
// completed but the form is still visible.
TEST_F(PasswordAutofillAgentTest,
       NoForm_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared) {
  const char kNoFormHTMLWithHiddenField[] =
      "<INPUT type='text' id='username'/>"
      "<INPUT type='password' id='password'/>"
      "<INPUT type='text' id='captcha' style='display:none'/>";
  LoadHTML(kNoFormHTMLWithHiddenField);

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_TRUE(captcha_element);

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  // Simulate captcha element show up right before AJAX completed.
  std::string show_captcha =
      "var captcha = document.getElementById('captcha');"
      "captcha.style = 'display:inline';";
  ExecuteJavaScriptForTests(show_captcha.c_str());

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_dynamic_form_submission());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

TEST_F(PasswordAutofillAgentTest,
       NoForm_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared_2) {
  const char kNoFormHTMLWithHiddenField[] =
      "<INPUT type='text' id='username'/>"
      "<INPUT type='password' id='password'/>"
      "<INPUT type='text' id='captcha' style='display:none'/>";
  LoadHTML(kNoFormHTMLWithHiddenField);

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_TRUE(captcha_element);

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  // Simulate captcha element show up right after AJAX completed.
  std::string show_captcha =
      "var captcha = document.getElementById('captcha');"
      "captcha.style = 'display:inline';";
  ExecuteJavaScriptForTests(show_captcha.c_str());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_driver_.called_dynamic_form_submission());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

// Tests that no save prompt is shown when a form with empty action URL is
// changed and AJAX completed but the form is still visible.
TEST_F(PasswordAutofillAgentTest,
       NoAction_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared) {
  // Form without an action URL.
  const char kHTMLWithHiddenField[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='text' id='captcha' style='display:none'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  // Set the valid URL so the form action URL can be generated properly.
  LoadHTMLWithUrlOverride(kHTMLWithHiddenField, "https://www.example.com");

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_TRUE(captcha_element);

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  // Simulate captcha element show up right before AJAX completed.
  captcha_element.SetAttribute("style", "display:inline;");

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_dynamic_form_submission());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

TEST_F(PasswordAutofillAgentTest,
       NoAction_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared_2) {
  // Form without an action URL.
  const char kHTMLWithHiddenField[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='text' id='captcha' style='display:none'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  // Set the valid URL so the form action URL can be generated properly.
  LoadHTMLWithUrlOverride(kHTMLWithHiddenField, "https://www.example.com");

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_TRUE(captcha_element);

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  // Simulate captcha element show up right after AJAX completed.
  captcha_element.SetAttribute("style", "display:inline;");

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_dynamic_form_submission());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutUnfillableField) {
  EXPECT_EQ(FocusedFieldType::kUnknown, fake_driver_.last_focused_field_type());
  FocusElement(kPasswordName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillablePasswordField,
            fake_driver_.last_focused_field_type());

  // Even though the focused element is a username field, it should be treated
  // as unfillable, since it is read-only.
  SetElementReadOnly(username_element_, true);
  FocusElement(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kUnfillableElement,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutFillableFields) {
  FocusElement("random_field");
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableNonSearchField,
            fake_driver_.last_focused_field_type());

  // A username field without fill data is indistinguishable from any other text
  // field.
  FocusElement(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableNonSearchField,
            fake_driver_.last_focused_field_type());

  FocusElement(kPasswordName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillablePasswordField,
            fake_driver_.last_focused_field_type());

  // A username field with fill data should be detected.
  SimulateOnFillPasswordForm(fill_data_);
  FocusElement(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableUsernameField,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutFillableSearchField) {
  LoadHTML(kSearchFieldHTML);
  FocusElement(kSearchField);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableSearchField,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordAutofillAgentTest,
       DriverInformedAboutWebAuthnIfNotPasswordOrUsername) {
  LoadHTML(kWebAutnFieldHTML);
  UpdateUrlForHTML(kWebAutnFieldHTML);
  UpdateUsernameAndPasswordElements();

  // Classify webauthn-tagged fields as webauthn if they aren't anything else.
  FocusElement(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableWebauthnTaggedField,
            fake_driver_.last_focused_field_type());

  // Don't classify password fields as webauthn. Fallbacks are the
  // same anyway.
  FocusElement(kPasswordName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillablePasswordField,
            fake_driver_.last_focused_field_type());

  // Once username fields are detectable, prefer username
  // classification.
  SimulateOnFillPasswordForm(fill_data_);
  FocusElement(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableUsernameField,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutFillableTextArea) {
  LoadHTML(kSocialNetworkPostFormHTML);

  FocusElement(kSocialMediaTextArea);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableTextArea,
            fake_driver_.last_focused_field_type());
}

// Tests that credential suggestions are autofilled on a password (and change
// password) forms having either ambiguous or empty name.
TEST_F(PasswordAutofillAgentTest,
       SuggestionsOnFormContainingAmbiguousOrEmptyNames) {
  const char kEmpty[] = "";
  const char kFormContainsEmptyNamesHTML[] =
      "<FORM name='WithoutNameIdForm' action='http://www.bidule.com' >"
      "  <INPUT type='text' placeholder='username'/>"
      "  <INPUT type='password' placeholder='Password'/>"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kFormContainsAmbiguousNamesHTML[] =
      "<FORM name='AmbiguousNameIdForm' action='http://www.bidule.com' >"
      "  <INPUT type='text' id='credentials' placeholder='username' />"
      "  <INPUT type='password' id='credentials' placeholder='Password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormContainsEmptyNamesHTML[] =
      "<FORM name='ChangePwd' action='http://www.bidule.com' >"
      "  <INPUT type='text' placeholder='username' />"
      "  <INPUT type='password' placeholder='Old Password' "
      "    autocomplete='current-password' />"
      "  <INPUT type='password' placeholder='New Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormButNoUsername[] =
      "<FORM name='ChangePwdButNoUsername' action='http://www.bidule.com' >"
      "  <INPUT type='password' placeholder='Old Password' "
      "    autocomplete='current-password' />"
      "  <INPUT type='password' placeholder='New Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormButNoOldPassword[] =
      "<FORM name='ChangePwdButNoOldPwd' action='http://www.bidule.com' >"
      "  <INPUT type='text' placeholder='username' />"
      "  <INPUT type='password' placeholder='New Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='password' placeholder='Retype Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormButNoAutocompleteAttribute[] =
      "<FORM name='ChangePwdButNoAutocomplete' action='http://www.bidule.com'>"
      "  <INPUT type='text' placeholder='username' />"
      "  <INPUT type='password' placeholder='Old Password' />"
      "  <INPUT type='password' placeholder='New Password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const struct {
    const char* html_form;
    bool does_trigger_autocomplete_on_fill;
    bool has_fillable_username;
    const char* expected_username_suggestions;
    const char* expected_password_suggestions;
    bool expected_is_username_autofillable;
    bool expected_is_password_autofillable;
  } test_cases[] = {
      // Password form without name or id attributes specified for the input
      // fields.
      {kFormContainsEmptyNamesHTML, true, true, kAliceUsername, kAlicePassword,
       true, true},

      // Password form with ambiguous name or id attributes specified for the
      // input fields.
      {kFormContainsAmbiguousNamesHTML, true, true, kAliceUsername,
       kAlicePassword, true, true},

      // Change password form without name or id attributes specified for the
      // input fields and `autocomplete='current-password'` attribute for old
      // password field.
      {kChangePasswordFormContainsEmptyNamesHTML, true, true, kAliceUsername,
       kAlicePassword, true, true},

      // Change password form without username field.
      {kChangePasswordFormButNoUsername, true, false, kEmpty, kAlicePassword,
       false, true},

      // Change password form without name or id attributes specified for the
      // input fields and `autocomplete='new-password'` attribute for new
      // password fields. This form *do not* trigger `OnFillPasswordForm` from
      // browser.
      {kChangePasswordFormButNoOldPassword, false, true, kEmpty, kEmpty, false,
       false},

      // Change password form without name or id attributes specified for the
      // input fields but `autocomplete='current-password'` or
      // `autocomplete='new-password'` attributes are missing for old and new
      // password fields respectively.
      {kChangePasswordFormButNoAutocompleteAttribute, true, true,
       kAliceUsername, kAlicePassword, true, true},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "html_form: " << test_case.html_form);

    // Load a password form.
    LoadHTML(test_case.html_form);
    UpdateUrlForHTML(test_case.html_form);

    // Get the username and password form input elements.
    blink::WebDocument document = GetMainFrame()->GetDocument();
    blink::WebVector<WebFormElement> forms = document.GetTopLevelForms();
    WebFormElement form_element = forms[0];
    std::vector<blink::WebFormControlElement> control_elements =
        form_util::GetOwnedAutofillableFormControls(document, form_element);
    if (test_case.has_fillable_username) {
      username_element_ = control_elements[0].To<WebInputElement>();
      password_element_ = control_elements[1].To<WebInputElement>();
    } else {
      username_element_.Reset();
      password_element_ = control_elements[0].To<WebInputElement>();
    }

    if (test_case.does_trigger_autocomplete_on_fill) {
      // Prepare `fill_data_` to trigger autocomplete.
      UpdateRendererIDsInFillData();
      fill_data_.additional_logins.clear();

      ClearUsernameAndPasswordFieldValues();

      // Simulate the browser sending back the login info, it triggers the
      // autocomplete.
      SimulateOnFillPasswordForm(fill_data_);

      if (test_case.has_fillable_username) {
        SimulateSuggestionChoice(username_element_);
      } else {
        SimulateSuggestionChoice(password_element_);
      }

      // The username and password should now have been autocompleted.
      CheckTextFieldsDOMState(test_case.expected_username_suggestions,
                              test_case.expected_is_username_autofillable,
                              test_case.expected_password_suggestions,
                              test_case.expected_is_password_autofillable);
    }
  }
}

// The password manager autofills credentials, the user chooses another
// credentials option from a suggestion dropdown and then the user submits a
// form. This test verifies that the browser process receives submitted
// username/password from the renderer process.
TEST_F(PasswordAutofillAgentTest, RememberChosenUsernamePassword) {
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoiceOfUsernameAndPassword(username_element_,
                                                kBobUsername16, kBobPassword16);

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent sends to the browser selected
  // credentials.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kBobUsername16, kBobPassword16,
      u"");
}

// Tests that we can correctly suggest to autofill two forms without username
// fields.
TEST_F(PasswordAutofillAgentTest, ShowSuggestionForNonUsernameFieldForms) {
  LoadHTML(kTwoNoUsernameFormsHTML);
  fill_data_.preferred_login.username_value.clear();
  UpdateUrlForHTML(kTwoNoUsernameFormsHTML);
  SimulateOnFillPasswordForm(fill_data_);

  ASSERT_TRUE(SimulateElementClick("password1"));
  CheckSuggestions(std::u16string(), true);
  ASSERT_TRUE(SimulateElementClick("password2"));
  CheckSuggestions(std::u16string(), true);
}

// Tests that password manager sees both autofill assisted and user entered
// data on saving that is triggered by AJAX succeeded.
TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_AJAXSucceeded) {
  for (auto change_source :
       {FieldChangeSource::USER, FieldChangeSource::AUTOFILL_SINGLE_FIELD,
        FieldChangeSource::USER_AUTOFILL_SINGLE_FIELD,
        FieldChangeSource::AUTOFILL_FORM,
        FieldChangeSource::USER_AUTOFILL_FORM}) {
    LoadHTML(kNoFormHTML);
    UpdateUsernameAndPasswordElements();

    SimulateUsernameTyping("Bob");
    SimulatePasswordTyping("mypassword");
    SimulateUsernameFieldChange(change_source);

    // Hide form elements to simulate successful login.
    std::string hide_elements =
        "var password = document.getElementById('password');"
        "password.style = 'display:none';"
        "var username = document.getElementById('username');"
        "username.style = 'display:none';";
    ExecuteJavaScriptForTests(hide_elements.c_str());

    FireAjaxSucceeded();

    ExpectSameDocumentNavigationWithUsernameAndPasswords(
        FormRendererId(), u"Alice", u"mypassword", u"",
        SubmissionIndicatorEvent::XHR_SUCCEEDED);
  }
}

TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_AJAXSucceeded_2) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");
  SimulateUsernameTyping("Alice");

  FireAjaxSucceeded();

  // Hide form elements to simulate successful login.
  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());
  ForceLayoutUpdate();
  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      FormRendererId(), u"Alice", u"mypassword", u"",
      SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

// Tests that password manager sees both autofill assisted and user entered
// data on saving that is triggered by form submission.
TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_FormSubmitted) {
  for (auto change_source :
       {FieldChangeSource::USER, FieldChangeSource::AUTOFILL_SINGLE_FIELD,
        FieldChangeSource::USER_AUTOFILL_SINGLE_FIELD,
        FieldChangeSource::AUTOFILL_FORM,
        FieldChangeSource::USER_AUTOFILL_FORM}) {
    LoadHTML(kFormHTML);
    UpdateUsernameAndPasswordElements();
    SimulateUsernameTyping("Bob");
    SimulatePasswordTyping("mypassword");
    SimulateUsernameFieldChange(change_source);

    SaveAndSubmitForm();

    ExpectFormSubmittedWithUsernameAndPasswords(
        GetFormUniqueRendererId("LoginTestForm"), u"Alice", u"mypassword", u"");
  }
}

// Tests that a suggestion dropdown is shown on a password field even if a
// username field is present.
TEST_F(PasswordAutofillAgentTest, SuggestPasswordFieldSignInForm) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the password element. This should produce a
  // dropdown with suggestion of all available usernames.
  SimulateElementClick(password_element_);
  CheckSuggestions(u"", true);
}

// TODO(crbug.com/40819370): Amend the test to port it on Android if possible.
// Otherwise, remove the TODO and add the reason why it is excluded.
#if !BUILDFLAG(IS_ANDROID)
// Tests that a suggestion dropdown is shown on each password field. But when a
// user chose one of the fields to autofill, a suggestion dropdown will be shown
// only on this field.
TEST_F(PasswordAutofillAgentTest, SuggestMultiplePasswordFields) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateUrlForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the password elements. This should produce
  // dropdowns with suggestion of all available usernames.
  ASSERT_TRUE(SimulateElementClick("password"));
  CheckSuggestions(u"", true);

  ASSERT_TRUE(SimulateElementClick("newpassword"));
  CheckSuggestions(u"", true);

  ASSERT_TRUE(SimulateElementClick("confirmpassword"));
  CheckSuggestions(u"", true);

  // The user chooses to autofill the current password field.
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions)
      .Times(NumShowSuggestionsCalls());
  SimulateElementClick(password_element_);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on not autofilled password fields. This should
  // produce no suggestion dropdowns.
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
  ASSERT_TRUE(SimulateElementClick("newpassword"));
  ASSERT_TRUE(SimulateElementClick("confirmpassword"));
  base::RunLoop().RunUntilIdle();

  // But when the user clicks on the autofilled password field again it should
  // still produce a suggestion dropdown.
  ASSERT_TRUE(SimulateElementClick("password"));
  CheckSuggestions(u"", true);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PasswordAutofillAgentTest, ShowAutofillSignaturesFlag) {
  // Tests that form signature is set iff the flag is enabled.
  const bool kFalseTrue[] = {false, true};
  for (bool show_signatures : kFalseTrue) {
    if (show_signatures)
      EnableShowAutofillSignatures();

    // An empty DOMSubtreeModified event listener is added for
    // https://crbug.com/1219852.
    std::string dom_with_dom_subtree_modified_listener =
        base::StrCat({"<SCRIPT>"
                      "window.addEventListener('DOMSubtreeModified', () => {});"
                      "</SCRIPT>",
                      kFormHTML});
    LoadHTML(dom_with_dom_subtree_modified_listener.c_str());
    WebDocument document = GetMainFrame()->GetDocument();
    WebFormElement form_element =
        document.GetElementById(WebString::FromASCII("LoginTestForm"))
            .To<WebFormElement>();
    ASSERT_TRUE(form_element);

    // Check only form signature attribute. The full test is in
    // "PasswordGenerationAgentTestForHtmlAnnotation.*".
    WebString form_signature_attribute = WebString::FromASCII("form_signature");
    EXPECT_EQ(form_element.HasAttribute(form_signature_attribute),
              show_signatures);
  }
}

// Checks that a same-document navigation form submission could have an empty
// username.
TEST_F(PasswordAutofillAgentTest,
       SameDocumentNavigationSubmissionUsernameIsEmpty) {
  username_element_.SetValue(WebString());
  SimulatePasswordTyping("random");
  FormRendererId renderer_id = GetFormUniqueRendererId("LoginTestForm");

  // Simulate that JavaScript removes the submitted form from DOM. That means
  // that a submission was successful.
  ExecuteJavaScriptForTests(kJavaScriptRemoveForm);

  FireDidFinishSameDocumentNavigation();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      renderer_id, std::u16string(), u"random", std::u16string(),
      SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
}

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
// Verify CheckSafeBrowsingReputation() is called when user starts filling
// a password field, and that this function is only called once.
TEST_F(PasswordAutofillAgentTest,
       CheckSafeBrowsingReputationWhenUserStartsFillingUsernamePassword) {
  ASSERT_EQ(0, fake_driver_.called_check_safe_browsing_reputation_cnt());
  // Simulate a click on password field to set its on focus,
  // CheckSafeBrowsingReputation() should be called.
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  // Subsequent editing will not trigger CheckSafeBrowsingReputation.
  SimulatePasswordTyping("modify");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  // No CheckSafeBrowsingReputation() call on username field click.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  // Navigate to another page and click on password field,
  // CheckSafeBrowsingReputation() should be triggered again.
  LoadHTML(kFormHTML);
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_driver_.called_check_safe_browsing_reputation_cnt());
}
#endif

// Tests that username/password are autofilled when JavaScript is changing url
// between discovering a form and receiving credentials from the browser
// process.
TEST_F(PasswordAutofillAgentTest, AutocompleteWhenPageUrlIsChanged) {
  // Simulate that JavaScript changes url.
  fill_data_.url = GURL(fill_data_.url.possibly_invalid_spec() + "/path");

  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

// Regression test for https://crbug.com/728028.
TEST_F(PasswordAutofillAgentTest, NoForm_MultipleAJAXEventsWithoutSubmission) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();

  // Repeatedly occurring AJAX events without removing the input elements
  // shouldn't be treated as a password submission.

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(fake_driver_.called_password_form_submitted());
  ASSERT_FALSE(static_cast<bool>(fake_driver_.form_data_submitted()));
}

TEST_F(PasswordAutofillAgentTest, ManualFallbackForSaving) {
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword).Times(0);
  // The users enters a username. Inform the driver regardless.
  SimulateUsernameTyping(kUsernameName);
  EXPECT_EQ(1, fake_driver_.called_inform_about_user_input_count());

  // The user enters a password.
  SimulatePasswordTyping(kPasswordName);
  // SimulateUsernameTyping/SimulatePasswordTyping calls
  // PasswordAutofillAgent::UpdateStateForTextChange only once.
  EXPECT_EQ(2, fake_driver_.called_inform_about_user_input_count());

  // Remove one character from the password value.
  SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
  EXPECT_EQ(3, fake_driver_.called_inform_about_user_input_count());

  // Add one character to the username value.
  SetFocused(username_element_);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(4, fake_driver_.called_inform_about_user_input_count());

  // Remove username value.
  SimulateUsernameTyping("");
  EXPECT_EQ(5, fake_driver_.called_inform_about_user_input_count());

  // Change the password.
  SetFocused(password_element_);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(6, fake_driver_.called_inform_about_user_input_count());

  // Remove password value. Inform the driver too.
  SimulatePasswordTyping("");
  EXPECT_EQ(7, fake_driver_.called_inform_about_user_input_count());

  // The user enters new password.
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(8, fake_driver_.called_inform_about_user_input_count());
}

TEST_F(PasswordAutofillAgentTest, ManualFallbackForSaving_PasswordChangeForm) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateUrlForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  // No password to save yet - still we should inform the driver.
  SimulateUsernameTyping(kUsernameName);
  EXPECT_EQ(1, fake_driver_.called_inform_about_user_input_count());

  // The user enters in the current password field. The fallback should be
  // available to save the entered value.
  SimulatePasswordTyping(kPasswordName);
  // SimulateUsernameTyping/SimulatePasswordTyping calls
  // PasswordAutofillAgent::UpdateStateForTextChange only once.
  EXPECT_EQ(2, fake_driver_.called_inform_about_user_input_count());

  // The user types into the new password field. Inform the driver.
  WebInputElement new_password = GetInputElementByID("newpassword");
  ASSERT_TRUE(new_password);
  SetFocused(new_password);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(3, fake_driver_.called_inform_about_user_input_count());

  // Edits of the confirmation password field trigger informing the driver.
  WebInputElement confirmation_password =
      GetInputElementByID("confirmpassword");
  ASSERT_TRUE(confirmation_password);
  SetFocused(confirmation_password);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(4, fake_driver_.called_inform_about_user_input_count());

  // Clear all password fields. The driver should be informed.
  SimulatePasswordTyping("");
  SimulateUserInputChangeForElement(new_password, "");
  SimulateUserInputChangeForElement(confirmation_password, "");
  EXPECT_EQ(5, fake_driver_.called_inform_about_user_input_count());
}

// Tests that information about Gaia reauthentication form is sent to the
// browser with information that the password should not be saved.
TEST_F(PasswordAutofillAgentTest, GaiaReauthenticationFormIgnored) {
  // HTML is already loaded in test SetUp method, so information about password
  // forms was already sent to the `fake_driver_`. Hence it should be reset.
  fake_driver_.reset_password_forms_calls();

  const char kGaiaReauthenticationFormHTML[] =
      "<FORM id='ReauthenticationForm'>"
      "  <INPUT type='hidden' name='continue' "
      "value='https://passwords.google.com/'>"
      "  <INPUT type='hidden' name='rart'>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";

  LoadHTMLWithUrlOverride(kGaiaReauthenticationFormHTML,
                          "https://accounts.google.com");
  UpdateOnlyPasswordElement();

  // Simulate a user clicking on the password element.
  SimulateElementClick(password_element_);

  fake_driver_.Flush();
  // Check that information about Gaia reauthentication is sent to the browser.
  ASSERT_TRUE(fake_driver_.called_password_forms_parsed());
  const std::vector<autofill::FormData>& parsed_form_data =
      fake_driver_.form_data_parsed().value();
  ASSERT_EQ(1u, parsed_form_data.size());
  EXPECT_TRUE(parsed_form_data[0].is_gaia_with_skip_save_password_form());
}

TEST_F(PasswordAutofillAgentTest,
       UpdateSuggestionsIfNewerCredentialsAreSupplied) {
  // Supply old fill data
  password_autofill_agent_->SetPasswordFillData(fill_data_);
  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  // Change fill data
  fill_data_.preferred_login.password_value = u"a-changed-password";
  // Supply changed fill data
  password_autofill_agent_->SetPasswordFillData(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, "a-changed-password",
                                true);
}

TEST_F(PasswordAutofillAgentTest, SuggestLatestCredentials) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  password_autofill_agent_->SetPasswordFillData(fill_data_);
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions)
      .Times(NumShowSuggestionsCalls());
  base::RunLoop().RunUntilIdle();

  // Change fill data
  fill_data_.preferred_login.username_value = u"a-changed-username";

  password_autofill_agent_->SetPasswordFillData(fill_data_);
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  // Empty value because nothing was typed into the field.
  CheckSuggestions(u"", true);
}

// Tests that PSL matched password is not autofilled even when there is
// a prefilled username.
TEST_F(PasswordAutofillAgentTest, PSLMatchedPasswordIsNotAutofill) {
  const char kFormWithPrefilledUsernameHTML[] =
      "<FORM id='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username' value='prefilledusername'/>"
      "  <INPUT type='password' id='password'/>"
      "</FORM>";
  LoadHTML(kFormWithPrefilledUsernameHTML);

  // Retrieve the input elements so the test can access them.
  UpdateUsernameAndPasswordElements();

  // Set the expected form origin and action URLs.
  UpdateUrlForHTML(kFormWithPrefilledUsernameHTML);

  // Add PSL matched credentials with username equal to prefilled one.
  PasswordAndMetadata psl_credentials;
  psl_credentials.password_value = u"pslpassword";
  // Non-empty realm means PSL matched credentials.
  psl_credentials.realm = "example.com";
  psl_credentials.username_value = u"prefilledusername";
  fill_data_.additional_logins.push_back(std::move(psl_credentials));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Test that PSL matched password is not autofilled.
  CheckUsernameDOMStatePasswordSuggestedState("prefilledusername", false, "",
                                              false);
}

// Tests that the password form is filled as expected on load.
TEST_F(PasswordAutofillAgentTest, FillOnLoadWith) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

TEST_F(PasswordAutofillAgentTest, FillOnLoadNoForm) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, FillOnLoadNoUsername) {
  LoadHTML(kTwoNoUsernameFormsHTML);
  username_element_.Reset();
  fill_data_.preferred_login.username_value.clear();
  password_element_ = GetInputElementByID("password2");
  UpdateRendererIDsInFillData();
  SimulateOnFillPasswordForm(fill_data_);
  EXPECT_EQ(kAlicePassword, password_element_.SuggestedValue().Utf8());
}

TEST_F(PasswordAutofillAgentTest, MayUsePlaceholderNoPlaceholder) {
  fill_data_.username_may_use_prefilled_placeholder = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest,
       MayUsePlaceholderAndPlaceholderOnFormDisabled) {
  username_element_.SetValue(WebString::FromUTF8("placeholder"));
  fill_data_.username_may_use_prefilled_placeholder = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("placeholder", false, "", false);
}

TEST_F(PasswordAutofillAgentTest,
       MayUsePlaceholderAndPlaceholderOnFormEnabled) {
  EnableOverwritingPlaceholderUsernames();
  username_element_.SetValue(WebString::FromUTF8("placeholder"));
  fill_data_.username_may_use_prefilled_placeholder = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, NoMayUsePlaceholderAndPlaceholderOnForm) {
  username_element_.SetValue(WebString::FromUTF8("placeholder"));
  fill_data_.username_may_use_prefilled_placeholder = false;

  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("placeholder", false, "", false);
}

TEST_F(PasswordAutofillAgentTest, AutofillsAfterUserGesture) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  password_autofill_agent_->UserGestureObserved();
  // It's a way to call PasswordValueGatekeeper::Reset().
  password_autofill_agent_->ReadyToCommitNavigation(nullptr);

  fill_data_.username_may_use_prefilled_placeholder = true;
  fill_data_.preferred_login.password_value = kBobPassword16;

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsStateForElements(
      username_element_, kAliceUsername,
      /* username_autofilled */ true, password_element_, kBobPassword,
      /* password_autofilled */ true, /* check_suggested_username */ false,
      /* check_suggested_username */ true);
  /// CheckTextFieldsSuggestedState(kAliceUsername, true, kBobPassword, true);
}

TEST_F(PasswordAutofillAgentTest, RestoresAfterJavaScriptModification) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  fake_driver_.reset_password_forms_calls();

  static const char script[] = "document.getElementById('username').value = ''";
  ExecuteJavaScriptForTests(script);
  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);

  password_autofill_agent_->OnDynamicFormsSeen();
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());
}

TEST_F(PasswordAutofillAgentTest, DoNotRestoreWhenFormStructureWasChanged) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  static const char clear_username_script[] =
      "document.getElementById('username').value = ''";
  ExecuteJavaScriptForTests(clear_username_script);
  static const char add_input_element_script[] =
      "document.getElementById('LoginTestForm').appendChild(document."
      "createElement('input'))";
  ExecuteJavaScriptForTests(add_input_element_script);
  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);

  password_autofill_agent_->OnDynamicFormsSeen();
  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);
}

// Tests that a single username is filled and is exposed to JavaScript only
// after user gesture.
TEST_F(PasswordAutofillAgentTest, FillOnLoadSingleUsername) {
  // Simulate filling single username by clearing password fill data.
  fill_data_.preferred_login.password_value.clear();
  fill_data_.password_element_renderer_id = autofill::FieldRendererId();

  SimulateOnFillPasswordForm(fill_data_);

  // The username should have been autofilled.
  CheckTextFieldsSuggestedState(kAliceUsername, true, std::string(), false);

  // However, it should have filled with the suggested value, it should not have
  // filled with DOM accessible value.
  CheckTextFieldsDOMState(std::string(), true, std::string(), false);

  // Simulate a user click so that the username field's real value is filled.
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), false);
}

// Tests that `PreviewSuggestion` properly previews the single username.
TEST_F(PasswordAutofillAgentTest, SingleUsernamePreviewSuggestion) {
  fill_data_.preferred_login.password_value.clear();
  fill_data_.password_element_renderer_id = autofill::FieldRendererId();
  // Simulate the browser sending the login info, but set `wait_for_username` to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsSuggestedState(kAliceUsername, true, std::string(), false);

  // Try previewing with a username different from the one that was initially
  // sent to the renderer.
  password_autofill_agent_->PreviewSuggestion(username_element_, kBobUsername16,
                                              kCarolPassword16);
  CheckTextFieldsSuggestedState(kBobUsername, true, std::string(), false);
}

// Tests that `FillSuggestion` properly fills the single username.
TEST_F(PasswordAutofillAgentTest, SingleUsernameFillSuggestion) {
  fill_data_.preferred_login.password_value.clear();
  fill_data_.password_element_renderer_id = autofill::FieldRendererId();
  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // After filling with the suggestion, the username field should be filled.
  SimulateElementClick(username_element_);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), false);
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(username_length, username_length);

  // Try Filling with a suggestion with a username different from the one that
  // was initially sent to the renderer.
  password_autofill_agent_->FillPasswordSuggestion(kBobUsername16,
                                                   kCarolPassword16);
  CheckTextFieldsDOMState(kBobUsername, true, std::string(), false);
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(username_length, username_length);
}

// Tests that `ClearPreview` properly clears previewed single username. The
// original selection range should stay untouched.
TEST_F(PasswordAutofillAgentTest, SingleUsernameClearPreview) {
  fill_data_.preferred_login.password_value.clear();
  fill_data_.password_element_renderer_id = autofill::FieldRendererId();
  ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
  ASSERT_TRUE(SimulateElementClick(kUsernameName));
  username_element_.SetSelectionRange(0, 0);

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername16, kAlicePassword16);
  password_autofill_agent_->ClearPreviewedForm();

  EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
  CheckTextFieldsDOMState("ali", true, std::string(), false);
  CheckUsernameSelection(0, 0);
}

// Fill on account select for credentials with empty usernames:
// Do not refill usernames if non-empty username is already selected.
TEST_F(PasswordAutofillAgentTest, NoUsernameCredential) {
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);

  const char kPasswordForEmptyUsernameCredential[] = "empty";
  const char16_t kPasswordForEmptyUsernameCredential16[] = u"empty";

  // Add a credential with an empty username.
  PasswordAndMetadata empty_username_credential;
  empty_username_credential.password_value =
      kPasswordForEmptyUsernameCredential16;
  empty_username_credential.username_value = u"";
  fill_data_.additional_logins.push_back(std::move(empty_username_credential));

  SimulateOnFillPasswordForm(fill_data_);
  ClearUsernameAndPasswordFieldValues();
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions)
      .Times(NumShowSuggestionsCalls());
  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, kAliceUsername16, kAlicePassword16);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions)
      .Times(NumShowSuggestionsCalls());
  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, u"", kPasswordForEmptyUsernameCredential16);

  CheckTextFieldsDOMState(kAliceUsername, true,
                          kPasswordForEmptyUsernameCredential, true);
}

// Tests that any fields that have user input are not refilled on the next
// call of FillPasswordForm.
TEST_F(PasswordAutofillAgentTest, NoRefillOfUserInput) {
  ClearUsernameAndPasswordFieldValues();
  SimulateOnFillPasswordForm(fill_data_);
  ASSERT_TRUE(SimulateElementClick(kPasswordName));
  SimulatePasswordTyping("newpwd");
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsStateForElements(username_element_, kAliceUsername, true,
                                  password_element_, "newpwd", false, false,
                                  false);
}

// Tests that a JavaScript submission (e.g. via removing the form from a DOM)
// gets registered following a autofill after user trigger.
TEST_F(PasswordAutofillAgentTest, XhrSubmissionAfterFillingSuggestion) {
  SimulateOnFillPasswordForm(fill_data_);

  SimulateSuggestionChoiceOfUsernameAndPassword(username_element_,
                                                kBobUsername16, kBobPassword16);

  // Simulate that JavaScript removes the submitted form from DOM. That means
  // that a submission was successful.
  ExecuteJavaScriptForTests(kJavaScriptRemoveForm);
  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      fill_data_.form_renderer_id, kBobUsername16, kBobPassword16,
      std::u16string(), SubmissionIndicatorEvent::DOM_MUTATION_AFTER_AUTOFILL);
}

// Tests that a JavaScript submission (e.g. via removing the form from a DOM)
// does not get registered following a mere autofill on page load. This is
// necessary, because we potentially fill many forms on pageload, which the user
// likely won't interact with.
TEST_F(PasswordAutofillAgentTest, NoXhrSubmissionAfterFillingOnPageload) {
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate that JavaScript removes the submitted form from DOM. That means
  // that a submission was successful.
  ExecuteJavaScriptForTests(kJavaScriptRemoveForm);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_dynamic_form_submission());
}

// Tests that user modifying the text field value results in notifying the
// browser.
TEST_F(PasswordAutofillAgentTest, ModifyNonPasswordField) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();

  EXPECT_CALL(fake_driver_,
              UserModifiedNonPasswordField(
                  form_util::GetFieldRendererId(username_element_),
                  std::u16string(kAliceUsername16),
                  /*autocomplete_attribute_has_username=*/true,
                  /*is_likely_otp=*/false));
  SimulateUsernameTyping(kAliceUsername);
}

// Tests that inputting 1 symbol value into a non-password field does not notify
// the browser.
TEST_F(PasswordAutofillAgentTest, ModifyNonPasswordFieldOneSymbol) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();

  EXPECT_CALL(fake_driver_, UserModifiedNonPasswordField).Times(0);
  SimulateUsernameTyping("1");
}

// Tests that inputting 101 symbols into a non-password field does not notify
// the browser.
TEST_F(PasswordAutofillAgentTest, ModifyNonPasswordFieldTooManySymbols) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();

  EXPECT_CALL(fake_driver_, UserModifiedNonPasswordField).Times(0);
  std::string not_username(101, 'a');
  SimulateUsernameTyping(not_username);
}

// Tests that user modifying a text field with an OTP autocomplete attribute
// results in notifying the browser correspondingly.
TEST_F(PasswordAutofillAgentTest, ModifyNonPasswordFieldOTPAutocomplete) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();
  username_element_.SetAttribute(
      "autocomplete",
      password_manager::constants::kAutocompleteOneTimePassword);

  EXPECT_CALL(fake_driver_,
              UserModifiedNonPasswordField(
                  form_util::GetFieldRendererId(username_element_),
                  std::u16string(kAliceUsername16),
                  /*autocomplete_attribute_has_username=*/false,
                  /*is_likely_otp=*/true));
  SimulateUsernameTyping(kAliceUsername);
}

// Tests that user modifying a text field with a name suggesting it's an OTP
// field results in notifying the browser correspondingly.
TEST_F(PasswordAutofillAgentTest, ModifyNonPasswordFieldOTPName) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();
  username_element_.SetAttribute("name", "test-one-time-pass");

  EXPECT_CALL(fake_driver_,
              UserModifiedNonPasswordField(
                  form_util::GetFieldRendererId(username_element_),
                  std::u16string(kAliceUsername16),
                  /*autocomplete_attribute_has_username=*/true,
                  /*is_likely_otp=*/true));
  SimulateUsernameTyping(kAliceUsername);
}

// Tests that user modifying the text field value does not notify the browser if
// the field has name shorter than kMinInputNameLengthForSingleUsername symbols.
TEST_F(PasswordAutofillAgentTest, ModifyNonPasswordFieldShortName) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();
  username_element_.SetAttribute("name", "i");
  username_element_.SetAttribute("id", "i");
  ASSERT_TRUE(username_element_.NameForAutofill().length() == 1);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40820173): User typing doesn't send focus events properly.
  FocusFirstInputElement();
#endif
  EXPECT_CALL(fake_driver_, UserModifiedNonPasswordField).Times(0);
  SimulateUserInputChangeForElement(username_element_, kAliceUsername);
}

// Tests that user modifying the text field value does not notify the browser if
// the field is labeled as a search field.
TEST_F(PasswordAutofillAgentTest, ModifySearchField) {
  LoadHTML(kSingleUsernameFormHTML);
  UpdateOnlyUsernameElement();
  username_element_.SetAttribute("name", "thesearchfield");

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40820173): User typing doesn't send focus events properly.
  FocusFirstInputElement();
#endif
  EXPECT_CALL(fake_driver_, UserModifiedNonPasswordField).Times(0);
  SimulateUserInputChangeForElement(username_element_, kAliceUsername);
}

// Tests that user inputs are propagated to the browser properly when a Shadow
// DOM tree starts between the <form> and <input> tags.
TEST_F(PasswordAutofillAgentTest,
       ProvisionalPasswordSavingWhenFormTagHostsShadowDom) {
  LoadHTML(kFormTagHostsShadowDomInputs);

  // Identify username and password elements.
  username_element_ =
      GetElementByID("un_host").ShadowRoot().FirstChild().To<WebInputElement>();
  ASSERT_TRUE(username_element_);
  password_element_ =
      GetElementByID("pw_host").ShadowRoot().FirstChild().To<WebInputElement>();
  ASSERT_TRUE(password_element_);

  // Simulate user modifying field values and ensure they are propagated to the
  // browser.
  username_element_.SetValue(WebString::FromUTF8(kAliceUsername));
  password_autofill_agent_->UpdatePasswordStateForTextChange(username_element_);
  fake_driver_.Flush();
  EXPECT_EQ(fake_driver_.called_inform_about_user_input_count(), 1);

  password_element_.SetValue(WebString::FromUTF8(kAlicePassword));
  password_autofill_agent_->UpdatePasswordStateForTextChange(password_element_);
  fake_driver_.Flush();
  EXPECT_EQ(fake_driver_.called_inform_about_user_input_count(), 2);

  ASSERT_TRUE(fake_driver_.form_data_maybe_submitted().has_value());
  FormData submitted_form = fake_driver_.form_data_maybe_submitted().value();
  EXPECT_EQ(submitted_form.name(), u"shadyform");
  EXPECT_TRUE(FormHasFieldWithValue(submitted_form, kAliceUsername16));
  EXPECT_TRUE(FormHasFieldWithValue(submitted_form, kAlicePassword16));
}

// Tests that passwords are filled properly on manual fallback when a Shadow
// DOM tree starts between the <form> and <input> tags.
TEST_F(PasswordAutofillAgentTest,
       PasswordSuggestionFillingWhenFormTagHostsShadowDom) {
  LoadHTML(kFormTagHostsShadowDomInputs);
  ASSERT_TRUE(UpdateFormElementsForFormHostingShadowDom());

  // Propagate fill data for filling on manual fallback.
  UpdateRendererIDsInFillData();
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  SimulateElementClick(password_element_);
  // Ensure that field are not filled on page load.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // Trigger filling and check filled values.
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that password generation works when a Shadow DOM tree starts between
// the <form> and <input> tags.
TEST_F(PasswordAutofillAgentTest, PasswordGenerationWhenFormTagHostsShadowDom) {
  LoadHTML(kFormTagHostsShadowDomInputs);
  ASSERT_TRUE(UpdateFormElementsForFormHostingShadowDom());

#if BUILDFLAG(IS_ANDROID)
  // Ensure TTF isn't shown when the user focuses the password field.
  SimulateElementClick(password_element_);
  password_autofill_agent_->KeyboardReplacingSurfaceClosed(
      /*show_virtual_keyboard=*/false);
#endif  // BUILDFLAG(IS_ANDROID)

  // Propagate fill data for filling on manual fallback.
  UpdateRendererIDsInFillData();
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate focusing the field and triggering password generation.
  SimulateElementClick(password_element_);
  base::test::TestFuture<const std::optional<
      ::autofill::password_generation::PasswordGenerationUIData>&>
      future_for_waiting;
  password_generation_->TriggeredGeneratePassword(
      future_for_waiting.GetCallback());
  EXPECT_TRUE(future_for_waiting.Wait());

  const std::u16string kPassword = u"GeneratedPass24";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(kPassword)));
  password_generation_->GeneratedPasswordAccepted(kPassword);

  // Check that the generated password is filled into form.
  EXPECT_EQ(password_element_.Value().Utf16(), kPassword);
}

// Test that password manager gets notified about JS inputs in password fields.
TEST_F(PasswordAutofillAgentTest, JSFieldModificationPasswordForm) {
  ASSERT_EQ(fake_driver_.called_inform_about_user_input_count(), 0);

  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  const std::string kJsUsername = "js-set-username";
  const std::string kJsPassword = "js-set-password";
  ExecuteJavaScriptForTests(R"(document.getElementById('username').value = ')" +
                            kJsUsername + R"(';
        document.getElementById('password').value = ')" +
                            kJsPassword + "';");
  fake_driver_.Flush();

  EXPECT_EQ(fake_driver_.called_inform_about_user_input_count(), 2);
  ASSERT_TRUE(fake_driver_.form_data_maybe_submitted().has_value());
  FormData form_data = fake_driver_.form_data_maybe_submitted().value();
  ASSERT_EQ(form_data.fields().size(), 3u);
  EXPECT_EQ(form_data.fields()[1].value(), base::ASCIIToUTF16(kJsUsername));
  EXPECT_EQ(form_data.fields()[2].value(), base::ASCIIToUTF16(kJsPassword));
}

// Test that password manager is not notified about JS inputs in non
// password related fields.
TEST_F(PasswordAutofillAgentTest, JSFieldModificationUnrelatedField) {
  ASSERT_EQ(fake_driver_.called_inform_about_user_input_count(), 0);

  // First field in `kFormHTML` is unrelated to passwords.
  ExecuteJavaScriptForTests(
      R"(document.getElementById('random_field').value = 'js-set-whatever';)");
  fake_driver_.Flush();

  EXPECT_EQ(fake_driver_.called_inform_about_user_input_count(), 0);
}

// Tests that the metric for the number of times form fill data is stored
// for a form is recorded correctly.
TEST_F(PasswordAutofillAgentTest, TimesReceivedFillDataForFormMetric) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate form fill data changing after form reparsing.
  fill_data_.username_element_renderer_id = FieldRendererId();
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate receiving form fill data that cannot be used, e.g. because
  // the fields are not present on the page.
  // Simulate form fill data changing after form reparsing.
  fill_data_.username_element_renderer_id = FieldRendererId(404);
  fill_data_.password_element_renderer_id = FieldRendererId(40404);
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate navigating to a new document.
  password_autofill_agent_->ReadyToCommitNavigation(nullptr);
  // The histogram should be recorded only for the form present on a
  // page.
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.TimesReceivedFillDataForForm", 2, 1);
}

// Tests that if a password form was focused before parsing happened,
// suggestions are shown to the user once the form is parsed on Desktop,
// but not on Android.
TEST_F(PasswordAutofillAgentTest,
       ShowSuggestionsOnParsingAutofocusedPasswordForm) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kShowSuggestionsOnAutofocus);

#if BUILDFLAG(IS_ANDROID)
  // A fixture needed to properly test suggestions showing on Android.
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);
  // The method above leaves the field focused, which is not needed for this
  // test.
  BlurElement(kUsernameName);
#endif  // BUILDFLAG(IS_ANDROID)

  FocusElement(kUsernameName);
  CheckSuggestionsNotShown();

  // Simulate receiving credentials for filling from the browser and verify that
  // suggestions are shown to the user.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
#if BUILDFLAG(IS_ANDROID)
  CheckSuggestionsNotShown();
#else
  CheckSuggestions(/*typed_username=*/u"", true);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Tests that if a password form supporting WebAuthn was focused before parsing
// happened, suggestions are shown to the user once the form is parsed on all
// platforms.
TEST_F(PasswordAutofillAgentTest,
       ShowSuggestionsOnParsingAutofocusedWebAuthnForm) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kShowSuggestionsOnAutofocus);
  LoadHTML(kWebAutnFieldHTML);
  UpdateUsernameAndPasswordElements();
  UpdateRendererIDsInFillData();

#if BUILDFLAG(IS_ANDROID)
  // A fixture needed to properly test suggestions showing on Android.
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);
  // The method above leaves the field focused, which is not needed for this
  // test.
  BlurElement(kUsernameName);
#endif  // BUILDFLAG(IS_ANDROID)

  FocusElement(kUsernameName);
  CheckSuggestionsNotShown();

  // Simulate receiving credentials for filling from the browser and verify that
  // suggestions are shown to the user.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  CheckSuggestions(/*typed_username=*/u"", true);
}

// Tests that if a password form is reparsed, suggestions are not shown
// automatically.
TEST_F(PasswordAutofillAgentTest,
       DoNotShowSuggestionsOnParsingFocusedFormSecondTime) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kShowSuggestionsOnAutofocus);

#if BUILDFLAG(IS_ANDROID)
  // A fixture needed to properly test suggestions showing on Android.
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);
  // The method above leaves the field focused, which is not needed for this
  // test.
  BlurElement(kUsernameName);
#endif  // BUILDFLAG(IS_ANDROID)

  // Simulate receiving fill data from the browser and user focusing the field
  // to see suggestions.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  SimulateElementClick(kUsernameName);
  CheckSuggestions(/*typed_username=*/u"", true);

  // Simulate receiving new fill data from the browser and check that
  // suggestions are not shown.
  fill_data_.preferred_login.username_value = u"new_username";
  SimulateOnFillPasswordForm(fill_data_);
  CheckSuggestionsNotShown();
}

// Tests that if a password form is not focused, suggestions are not shown to
// the user once the form is parsed.
TEST_F(PasswordAutofillAgentTest,
       DoNotShowSuggestionsOnParsingFormWithoutFocus) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kShowSuggestionsOnAutofocus);

#if BUILDFLAG(IS_ANDROID)
  // A fixture needed to properly test suggestions showing on Android.
  SimulateClosingKeyboardReplacingSurfaceIfAndroid(kUsernameName);
  // The method above leaves the field focused, which is not needed for this
  // test.
  BlurElement(kUsernameName);
#endif  // BUILDFLAG(IS_ANDROID)

  // Simulate receiving credentials for filling from the browser.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  CheckSuggestionsNotShown();
}

TEST_F(PasswordAutofillAgentTest, InformingBrowserAboutUsernameTextFields) {
  LoadHTML(kSingleTextInputFormHTML);
  UpdateOnlyUsernameElement();

  // Simulate the browser parsing the text field as username and sending the
  // correspondent fill data.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  SimulateUsernameTyping(kUsernameName);
  EXPECT_EQ(1, fake_driver_.called_inform_about_user_input_count());
}

TEST_F(PasswordAutofillAgentTest, InformingBrowserAboutIrrelevantTextFields) {
  LoadHTML(kSingleTextInputFormHTML);
  UpdateOnlyUsernameElement();

  // The users types into a field that was not parsed as username.
  SimulateUsernameTyping(kUsernameName);
  EXPECT_EQ(0, fake_driver_.called_inform_about_user_input_count());
}

// Tests that fields with banned fields do not show password suggestions.
TEST_F(PasswordAutofillAgentTest, NoFillingFallbackForBannedFields) {
  // Form consists both of credential and credit card fields.
  LoadHTML(
      R"(
        <input type="text" id="username-field" name="username-field">
        <input type="password" id="password-field" name="password-field">
        <input type="text" id="credit-card-full-name"
          name="credit-card-full-name" placeholder="Full Name">
        <input type="password" id="credit-card-number"
          name="credit-card-number" placeholder="Card number">
        <input type="password" id="credit-card-cvc" name="credit-card-cvc"
          placeholder="CVC">
    )");
  WebInputElement username_field = GetInputElementByID("username-field");
  WebInputElement password_field = GetInputElementByID("username-field");
  WebInputElement credit_card_full_name_field =
      GetInputElementByID("credit-card-full-name");
  WebInputElement credit_card_number_field =
      GetInputElementByID("credit-card-number");
  WebInputElement credit_card_cvc_field =
      GetInputElementByID("credit-card-cvc");

  // Password Manager found credential fields and has saved credentials.
  PasswordFormFillData form_data;
  form_data.form_renderer_id = FormRendererId();
  form_data.username_element_renderer_id = FieldRef(username_field).GetId();
  form_data.password_element_renderer_id = FieldRef(password_field).GetId();
  form_data.preferred_login.username_value = kAliceUsername16;
  form_data.preferred_login.password_value = kAlicePassword16;
  form_data.suggestion_banned_fields = {
      FieldRef(credit_card_full_name_field).GetId(),
      FieldRef(credit_card_number_field).GetId(),
      FieldRef(credit_card_cvc_field).GetId()};
  password_autofill_agent_->SetPasswordFillData(form_data);

  // Expect filling suggestion on credential forms.
  EXPECT_TRUE(password_autofill_agent_->ShowSuggestions(
      username_field,
      AutofillSuggestionTriggerSource::kFormControlElementClicked));
  EXPECT_TRUE(password_autofill_agent_->ShowSuggestions(
      password_field,
      AutofillSuggestionTriggerSource::kFormControlElementClicked));
  // Expect no filling suggestion on credit card forms.
  EXPECT_FALSE(password_autofill_agent_->ShowSuggestions(
      credit_card_full_name_field,
      AutofillSuggestionTriggerSource::kFormControlElementClicked));
  EXPECT_FALSE(password_autofill_agent_->ShowSuggestions(
      credit_card_number_field,
      AutofillSuggestionTriggerSource::kFormControlElementClicked));
  EXPECT_FALSE(password_autofill_agent_->ShowSuggestions(
      credit_card_cvc_field,
      AutofillSuggestionTriggerSource::kFormControlElementClicked));
}

#if BUILDFLAG(IS_ANDROID)
// If a password field is hidden, the field unlikely has an Enter listener. So,
// trigger a form submission on the username field.
TEST_F(PasswordAutofillAgentTest, TriggerFormSubmission_HiddenPasswordField) {
  const char kUsernameFirstFormHTML[] =
      "<script>"
      "  function on_keypress(event) {"
      "    if (event.which === 13) {"
      "      var field = document.getElementById('password');"
      "      field.parentElement.removeChild(field);"
      "    }"
      "  }"
      "</script>"
      "<INPUT type='text' id='username' onkeypress='on_keypress(event)'/>"
      "<INPUT type='password' id='password' style='display:none'/>";
  LoadHTML(kUsernameFirstFormHTML);
  base::RunLoop().RunUntilIdle();
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled because the test
  // simulates filling with `FillSuggestion`, the function that
  // KeyboardReplacingSurface uses.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Fill the form.
  SimulateElementClick(username_element_);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  base::RunLoop().RunUntilIdle();

  // Trigger a form submission.
  password_autofill_agent_->TriggerFormSubmission();
  base::RunLoop().RunUntilIdle();

  // Verify that the driver actually has seen a submission.
  EXPECT_TRUE(fake_driver_.called_dynamic_form_submission());
}

class PasswordAutofillAgentFormPresenceVariationTest
    : public PasswordAutofillAgentTest,
      public testing::WithParamInterface<bool> {};

TEST_P(PasswordAutofillAgentFormPresenceVariationTest, TriggerFormSubmission) {
  bool has_form_tag = GetParam();

  LoadHTML(has_form_tag ? kFormHTML : kNoFormHTML);
  base::RunLoop().RunUntilIdle();
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending the login info, but set `wait_for_username`
  // to prevent the form from being immediately filled because the test
  // simulates filling with `FillSuggestion`, the function that
  // KeyboardReplacingSurface uses.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Fill the form.
  SimulateElementClick(username_element_);
  password_autofill_agent_->FillPasswordSuggestion(kAliceUsername16,
                                                   kAlicePassword16);
  base::RunLoop().RunUntilIdle();

  // Trigger a form submission.
  password_autofill_agent_->TriggerFormSubmission();
  base::RunLoop().RunUntilIdle();

  // Verify that the driver actually has seen a submission.
  if (has_form_tag)
    EXPECT_TRUE(fake_driver_.called_password_form_submitted());
  else
    EXPECT_TRUE(fake_driver_.called_dynamic_form_submission());

  fake_driver_.reset_password_forms_calls();
}

INSTANTIATE_TEST_SUITE_P(FormPresenceVariation,
                         PasswordAutofillAgentFormPresenceVariationTest,
                         testing::Bool());

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace autofill
