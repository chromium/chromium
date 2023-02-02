// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/renderer/autofill/fake_mojo_password_manager_driver.h"
#include "chrome/renderer/autofill/fake_password_generation_driver.h"
#include "chrome/renderer/autofill/password_generation_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/events/keycodes/keyboard_codes.h"

using autofill::mojom::FocusedFieldType;
using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebElement;
using blink::WebInputElement;
using blink::WebNode;
using blink::WebString;
using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::Eq;

namespace autofill {

namespace {

// Utility method that tries to find a field in `form` whose `id_attribute`
// matches `id`. Returns nullptr if no such field exists.
const FormFieldData* FindFieldById(const FormData& form, base::StringPiece id) {
  auto it = base::ranges::find(form.fields, base::UTF8ToUTF16(id),
                               &FormFieldData::id_attribute);
  return it != form.fields.end() ? &*it : nullptr;
}

}  // namespace

constexpr char kSigninFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

constexpr char kAccountCreationFormHTML[] =
    "<FORM id = 'blah' action = 'http://www.random.com/pa/th?q=1&p=3#first'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'text' id = 'address'/> "
    "  <INPUT type = 'text' id = 'hidden' style='display: none;'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

constexpr char kAccountCreationNoForm[] =
    "<INPUT type = 'text' id = 'username'/> "
    "<INPUT type = 'password' id = 'first_password' size = 5/>"
    "<INPUT type = 'password' id = 'second_password' size = 5/> "
    "<INPUT type = 'text' id = 'address'/> "
    "<INPUT type = 'text' id = 'hidden' style='display: none;'/> "
    "<INPUT type = 'button' id = 'dummy'/> "
    "<INPUT type = 'submit' value = 'LOGIN' />";

#if !BUILDFLAG(IS_ANDROID)
constexpr char kAccountCreationNoIds[] =
    "<FORM action = 'http://www.random.com/pa/th?q=1&p=3#first'> "
    "  <INPUT type = 'text'/> "
    "  <INPUT type = 'password' class='first_password'/>"
    "  <INPUT type = 'password' class='second_password'/> "
    "  <INPUT type = 'text'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN'/>"
    "</FORM>";
#endif  // !BUILDFLAG(IS_ANDROID)

constexpr char kDisabledElementAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete = 'off' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'text' id = 'address'/> "
    "  <INPUT type = 'text' id = 'disabled' disabled/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

constexpr char kHiddenPasswordAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password'/> "
    "  <INPUT type = 'password' id = 'second_password' style='display:none'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

constexpr char kMultipleAccountCreationFormHTML[] =
    "<FORM name = 'login' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'random'/> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>"
    "<FORM name = 'signup' action = 'http://www.random.com/signup'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete = 'off' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'text' id = 'address'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

constexpr char kPasswordChangeFormHTML[] =
    "<FORM name = 'ChangeWithUsernameForm' action = 'http://www.bidule.com'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'password' id = 'newpassword'/> "
    "  <INPUT type = 'password' id = 'confirmpassword'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'Login'/> "
    "</FORM>";

constexpr char kPasswordFormAndSpanHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/pa/th?q=1&p=3#first'>"
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "</FORM>"
    "<SPAN id='span'>Text to click on</SPAN>";

class PasswordGenerationAgentTest : public ChromeRenderViewTest {
 public:
  enum AutomaticGenerationStatus {
    kNotReported,
    kAvailable,
  };

  PasswordGenerationAgentTest() = default;

  PasswordGenerationAgentTest(const PasswordGenerationAgentTest&) = delete;
  PasswordGenerationAgentTest& operator=(const PasswordGenerationAgentTest&) =
      delete;

  // ChromeRenderViewTest:
  void RegisterMainFrameRemoteInterfaces() override;
  void SetUp() override;
  void TearDown() override;

  void LoadHTMLWithUserGesture(const char* html);
  WebElement GetElementById(base::StringPiece element_id);
  void FocusField(const char* element_id);

  void ExpectAutomaticGenerationAvailable(const char* element_id,
                                          AutomaticGenerationStatus available);
  void ExpectGenerationElementLostFocus(const char* new_element_id);
  void ExpectFormClassifierVoteReceived(
      bool received,
      const std::u16string& expected_generation_element);
  void SelectGenerationFallbackAndExpect(bool available);
  void ExpectAttribute(const WebElement& element,
                       base::StringPiece attribute,
                       base::StringPiece expected_value);

  void BindPasswordManagerDriver(mojo::ScopedInterfaceEndpointHandle handle);
  void BindPasswordManagerClient(mojo::ScopedInterfaceEndpointHandle handle);

  // Callback for TriggeredGeneratePassword.
  MOCK_METHOD1(TriggeredGeneratePasswordReply,
               void(const absl::optional<
                    autofill::password_generation::PasswordGenerationUIData>&));

  FakeMojoPasswordManagerDriver fake_driver_;
  testing::StrictMock<FakePasswordGenerationDriver> fake_pw_client_;
};

void PasswordGenerationAgentTest::RegisterMainFrameRemoteInterfaces() {
  // Because the test cases only involve the main frame in this test,
  // the fake password client is only used for the main frame.
  blink::AssociatedInterfaceProvider* remote_associated_interfaces =
      GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
  remote_associated_interfaces->OverrideBinderForTesting(
      mojom::PasswordGenerationDriver::Name_,
      base::BindRepeating(
          &PasswordGenerationAgentTest::BindPasswordManagerClient,
          base::Unretained(this)));
  remote_associated_interfaces->OverrideBinderForTesting(
      mojom::PasswordManagerDriver::Name_,
      base::BindRepeating(
          &PasswordGenerationAgentTest::BindPasswordManagerDriver,
          base::Unretained(this)));
}

void PasswordGenerationAgentTest::SetUp() {
  ChromeRenderViewTest::SetUp();

  // TODO(crbug/862989): Remove workaround preventing non-test classes to bind
  // fake_driver_ or fake_pw_client_.
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

  // Necessary for focus changes to work correctly and dispatch blur events
  // when a field was previously focused.
  GetWebFrameWidget()->SetFocus(true);
}

void PasswordGenerationAgentTest::TearDown() {
  // Unloading the document may trigger the event.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus()).Times(AtMost(1));
  ChromeRenderViewTest::TearDown();
}

void PasswordGenerationAgentTest::LoadHTMLWithUserGesture(const char* html) {
  LoadHTML(html);

  // Enable show-ime event when element is focused by indicating that a user
  // gesture has been processed since load.
  EXPECT_TRUE(SimulateElementClick("dummy"));
}

WebElement PasswordGenerationAgentTest::GetElementById(
    base::StringPiece element_id) {
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(
      blink::WebString::FromUTF8(element_id.data(), element_id.size()));
  CHECK(!element.IsNull());
  return element;
}

void PasswordGenerationAgentTest::FocusField(const char* element_id) {
  GetElementById(element_id);  // Just check the element exists.
  ExecuteJavaScriptForTests(
      base::StringPrintf("document.getElementById('%s').focus();", element_id)
          .c_str());
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1293802): On Android, the JS above doesn't trigger the
  // method below.
  GetMainFrame()->AutofillClient()->DidCompleteFocusChangeInFrame();
#endif  // BUILDFLAG(IS_ANDROID)
}

void PasswordGenerationAgentTest::ExpectAutomaticGenerationAvailable(
    const char* element_id,
    AutomaticGenerationStatus status) {
  SCOPED_TRACE(testing::Message()
               << "element_id = " << element_id << " available = " << status);
  if (status == kNotReported) {
    EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_)).Times(0);
  } else {
    EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  }

  FocusField(element_id);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Check that aria-autocomplete attribute is set correctly.
  if (status == kAvailable) {
    WebElement element = GetElementById(element_id);
    ExpectAttribute(element, "aria-autocomplete", "list");
  }
}

void PasswordGenerationAgentTest::ExpectGenerationElementLostFocus(
    const char* new_element_id) {
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
  FocusField(new_element_id);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
}

void PasswordGenerationAgentTest::ExpectFormClassifierVoteReceived(
    bool received,
    const std::u16string& expected_generation_element) {
  base::RunLoop().RunUntilIdle();
  if (received) {
    ASSERT_TRUE(fake_driver_.called_save_generation_field());
    EXPECT_EQ(expected_generation_element,
              fake_driver_.save_generation_field());
  } else {
    ASSERT_FALSE(fake_driver_.called_save_generation_field());
  }

  fake_driver_.reset_save_generation_field();
}

void PasswordGenerationAgentTest::SelectGenerationFallbackAndExpect(
    bool available) {
  if (available) {
    EXPECT_CALL(*this,
                TriggeredGeneratePasswordReply(testing::Ne(absl::nullopt)));
  } else {
    EXPECT_CALL(*this,
                TriggeredGeneratePasswordReply(testing::Eq(absl::nullopt)));
  }
  password_generation_->TriggeredGeneratePassword(base::BindOnce(
      &PasswordGenerationAgentTest::TriggeredGeneratePasswordReply,
      base::Unretained(this)));
  testing::Mock::VerifyAndClearExpectations(this);
}

void PasswordGenerationAgentTest::ExpectAttribute(
    const WebElement& element,
    base::StringPiece attribute,
    base::StringPiece expected_value) {
  WebString actual_value = element.GetAttribute(
      blink::WebString::FromUTF8(attribute.data(), attribute.size()));
  ASSERT_FALSE(actual_value.IsNull());
  EXPECT_EQ(expected_value, actual_value.Ascii());
}

void PasswordGenerationAgentTest::BindPasswordManagerDriver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  fake_driver_.BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::PasswordManagerDriver>(
          std::move(handle)));
}

void PasswordGenerationAgentTest::BindPasswordManagerClient(
    mojo::ScopedInterfaceEndpointHandle handle) {
  fake_pw_client_.BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::PasswordGenerationDriver>(
          std::move(handle)));
}

// Tests HTML forms' annotations (e.g. signatures, visibility). The parser's
// annotations are tested in PasswordManagerBrowserTest.ParserAnnotations.
class PasswordGenerationAgentTestForHtmlAnnotation
    : public PasswordGenerationAgentTest {
 public:
  PasswordGenerationAgentTestForHtmlAnnotation() = default;

  PasswordGenerationAgentTestForHtmlAnnotation(
      const PasswordGenerationAgentTestForHtmlAnnotation&) = delete;
  PasswordGenerationAgentTestForHtmlAnnotation& operator=(
      const PasswordGenerationAgentTestForHtmlAnnotation&) = delete;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kShowAutofillSignatures);
    PasswordGenerationAgentTest::SetUp();
  }

  void TestAnnotateForm(bool has_form_tag);
};

void PasswordGenerationAgentTestForHtmlAnnotation::TestAnnotateForm(
    bool has_form_tag) {
  SCOPED_TRACE(testing::Message() << "has_form_tag = " << has_form_tag);
  const char* kHtmlForm =
      has_form_tag ? kAccountCreationFormHTML : kAccountCreationNoForm;
  LoadHTMLWithUserGesture(kHtmlForm);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
  WebDocument document = GetMainFrame()->GetDocument();

  const char* kFormSignature =
      has_form_tag ? "17583149382422306905" : "17876980055561084954";
  if (has_form_tag) {
    // Check the form signature is set in the <form> tag.
    WebElement form_element = GetElementById("blah");
    ExpectAttribute(form_element, "form_signature", kFormSignature);
  }

  // Check field signatures and form signature are set in the <input>s.
  WebElement username_element = GetElementById("username");
  ExpectAttribute(username_element, "field_signature", "239111655");
  ExpectAttribute(username_element, "form_signature", kFormSignature);
  ExpectAttribute(username_element, "visibility_annotation", "true");

  WebElement password_element = GetElementById("first_password");
  ExpectAttribute(password_element, "field_signature", "3933215845");
  ExpectAttribute(password_element, "form_signature", kFormSignature);
  ExpectAttribute(password_element, "visibility_annotation", "true");
  ExpectAttribute(password_element, "password_creation_field", "1");

  WebElement hidden_element = GetElementById("hidden");
  ExpectAttribute(hidden_element, "visibility_annotation", "false");
}

TEST_F(PasswordGenerationAgentTest, HiddenSecondPasswordDetectionTest) {
  // Hidden fields are not treated differently.
  LoadHTMLWithUserGesture(kHiddenPasswordAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
}

TEST_F(PasswordGenerationAgentTest, DetectionTestNoForm) {
  LoadHTMLWithUserGesture(kAccountCreationNoForm);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);

  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
  ExpectGenerationElementLostFocus("second_password");
}

TEST_F(PasswordGenerationAgentTest, FillTest) {
  // Add event listeners for password fields.
  std::vector<std::u16string> variables_to_check;
  std::string events_registration_script =
      CreateScriptToRegisterListeners("first_password", &variables_to_check) +
      CreateScriptToRegisterListeners("second_password", &variables_to_check);

  // Make sure that we are enabled before loading HTML.
  std::string html =
      std::string(kAccountCreationFormHTML) + events_registration_script;
  // Begin with no gesture and therefore no focused element.
  LoadHTMLWithUserGesture(html.c_str());
  WebDocument document = GetMainFrame()->GetDocument();
  SetFoundFormEligibleForGeneration(password_generation_,
                                    GetMainFrame()->GetDocument(),
                                    /*new_password_id=*/"first_password",
                                    /*confirm_password_id=*/"second_password");
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  WebElement element =
      document.GetElementById(WebString::FromUTF8("first_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement first_password_element = element.To<WebInputElement>();
  element = document.GetElementById(WebString::FromUTF8("second_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement second_password_element = element.To<WebInputElement>();

  // Both password fields should be empty.
  EXPECT_TRUE(first_password_element.Value().IsNull());
  EXPECT_TRUE(second_password_element.Value().IsNull());

  std::u16string password = u"random_password";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));

  password_generation_->GeneratedPasswordAccepted(password);

  // Password fields are filled out and set as being autofilled.
  EXPECT_EQ(password, first_password_element.Value().Utf16());
  EXPECT_EQ(password, second_password_element.Value().Utf16());
  EXPECT_TRUE(first_password_element.IsAutofilled());
  EXPECT_TRUE(second_password_element.IsAutofilled());

  // Make sure all events are called.
  for (const std::u16string& variable : variables_to_check) {
    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(variable, &value));
    EXPECT_EQ(1, value) << variable;
  }

  // Check that focus returns to previously focused element.
  element = document.GetElementById(WebString::FromUTF8("address"));
  ASSERT_FALSE(element.IsNull());
  EXPECT_EQ(element, document.FocusedElement());
}

TEST_F(PasswordGenerationAgentTest, EditingTest) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(password_generation_,
                                    GetMainFrame()->GetDocument(),
                                    /*new_password_id=*/"first_password",
                                    /*confirm_password_id=*/"second_password");
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("first_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement first_password_element = element.To<WebInputElement>();
  element = document.GetElementById(WebString::FromUTF8("second_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement second_password_element = element.To<WebInputElement>();

  std::u16string password = u"random_password";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));

  password_generation_->GeneratedPasswordAccepted(password);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Passwords start out the same.
  EXPECT_EQ(password, first_password_element.Value().Utf16());
  EXPECT_EQ(password, second_password_element.Value().Utf16());

  // After editing the first field they are still the same.
  std::string edited_password_ascii = "edited_password";
  std::u16string edited_password = base::ASCIIToUTF16(edited_password_ascii);
  EXPECT_CALL(fake_pw_client_,
              PresaveGeneratedPassword(_, Eq(edited_password)));
  EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
  SimulateUserInputChangeForElement(&first_password_element,
                                    edited_password_ascii);
  EXPECT_EQ(edited_password, first_password_element.Value().Utf16());
  EXPECT_EQ(edited_password, second_password_element.Value().Utf16());
  EXPECT_TRUE(first_password_element.IsAutofilled());
  EXPECT_TRUE(second_password_element.IsAutofilled());
  ASSERT_TRUE(fake_driver_.form_data_maybe_submitted().has_value());
  EXPECT_THAT(FindFieldById(*fake_driver_.form_data_maybe_submitted(),
                            "first_password"),
              testing::Field(&FormFieldData::value, edited_password));
  EXPECT_THAT(FindFieldById(*fake_driver_.form_data_maybe_submitted(),
                            "second_password"),
              testing::Field(&FormFieldData::value, edited_password));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1293802): On Android, |SimulateUserInputChangeForElement|
  // doesn't trigger focusing events and |ShowPasswordEditingPopup| expectation
  // is unfulfilled. So, trigger it explicitly.
  FocusField("first_password");
#endif
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Verify that password mirroring works correctly even when the password
  // is deleted.
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(_));
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  SimulateUserInputChangeForElement(&first_password_element, std::string());
  EXPECT_EQ(std::u16string(), first_password_element.Value().Utf16());
  EXPECT_EQ(std::u16string(), second_password_element.Value().Utf16());
  EXPECT_FALSE(first_password_element.IsAutofilled());
  EXPECT_FALSE(second_password_element.IsAutofilled());
  ASSERT_TRUE(fake_driver_.form_data_maybe_submitted().has_value());
  EXPECT_THAT(FindFieldById(*fake_driver_.form_data_maybe_submitted(),
                            "first_password"),
              testing::Field(&FormFieldData::value, std::u16string()));
  EXPECT_THAT(FindFieldById(*fake_driver_.form_data_maybe_submitted(),
                            "second_password"),
              testing::Field(&FormFieldData::value, std::u16string()));
}

TEST_F(PasswordGenerationAgentTest, EditingEventsTest) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);

  // Generate password.
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
  std::u16string password = u"random_password";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Start removing characters one by one and observe the events sent to the
  // browser.
  EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
  FocusField("first_password");
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
  size_t max_chars_to_delete_before_editing =
      password.length() -
      PasswordGenerationAgent::kMinimumLengthForEditedPassword;
  for (size_t i = 0; i < max_chars_to_delete_before_editing; ++i) {
    password.erase(password.end() - 1);
    EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
    fake_pw_client_.Flush();
    fake_driver_.Flush();
    EXPECT_EQ(FocusedFieldType::kFillablePasswordField,
              fake_driver_.last_focused_field_type());
    testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
  }

  // Delete one more character and move back to the generation state.
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(_));
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
  fake_pw_client_.Flush();
  // Last focused element shouldn't change while editing.
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillablePasswordField,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordGenerationAgentTest, UnblocklistedMultipleTest) {
  // Receive two not blocklisted messages, one is for account creation form and
  // the other is not. Show password generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
}

TEST_F(PasswordGenerationAgentTest, AccountCreationFormsDetectedTest) {
  // Did not receive account creation forms detected message. Don't show
  // password generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);

  // Receive the account creation forms detected message. Show password
  // generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
}

TEST_F(PasswordGenerationAgentTest, MaximumCharsForGenerationOffer) {
  base::HistogramTester histogram_tester;

  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  // There should now be a message to show the UI.
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("first_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement first_password_element = element.To<WebInputElement>();

  // Make a password just under maximum offer size.
  // Due to implementation details it's OK to get one more trigger for the
  // automatic generation.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_))
      .Times(AtMost(1));
  SimulateUserInputChangeForElement(
      &first_password_element,
      std::string(PasswordGenerationAgent::kMaximumCharsForGenerationOffer,
                  'a'));
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Simulate a user typing a password just over maximum offer size.
  EXPECT_CALL(fake_pw_client_, PasswordGenerationRejectedByTyping());
  SimulateUserTypingASCIICharacter('a', true);
  // There should now be a message that generation was rejected.
  fake_pw_client_.Flush();

  // Simulate the user deleting characters. The generation popup should be
  // shown again.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Change focus. Bubble should be hidden.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
  FocusField("username");
  fake_pw_client_.Flush();

  // Focusing the password field will bring up the generation UI again.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  FocusField("first_password");
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Loading a different page triggers UMA stat upload. Verify that only one
  // display event is sent.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
  LoadHTMLWithUserGesture(kSigninFormHTML);
  fake_pw_client_.Flush();

  histogram_tester.ExpectBucketCount(
      "PasswordGeneration.Event",
      autofill::password_generation::GENERATION_POPUP_SHOWN, 1);
}

TEST_F(PasswordGenerationAgentTest,
       MaximumCharsForGenerationOfferIgnoredWithEnabledStrengthIndicator) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordStrengthIndicator);

  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("first_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement first_password_element = element.To<WebInputElement>();

  // Dropdown should not be hidden with more characters than maximum for
  // generation when strength indicator feature is enabled.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_))
      .Times(AtMost(1));
  SimulateUserInputChangeForElement(
      &first_password_element,
      std::string(PasswordGenerationAgent::kMaximumCharsForGenerationOffer + 1,
                  'a'));
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Dropdown should be hidden when focus changes.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
  FocusField("username");
  fake_pw_client_.Flush();

  // Focusing the password field should bring the generation dropdown again.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  FocusField("first_password");
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
}

TEST_F(PasswordGenerationAgentTest, MinimumLengthForEditedPassword) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  // Generate a new password.
  std::u16string password = u"random_password";

  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Delete most of the password.
  EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_)).Times(0);
  FocusField("first_password");
  size_t max_chars_to_delete =
      password.length() -
      PasswordGenerationAgent::kMinimumLengthForEditedPassword;
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, _))
      .Times(testing::AtLeast(1));
  for (size_t i = 0; i < max_chars_to_delete; ++i)
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, false);
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Delete one more character. The state should move to offering generation.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(testing::_));
  SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // The first password field is still non empty. The second one should be
  // cleared.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("first_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement first_password_element = element.To<WebInputElement>();
  element = document.GetElementById(WebString::FromUTF8("second_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement second_password_element = element.To<WebInputElement>();
  EXPECT_NE(std::u16string(), first_password_element.Value().Utf16());
  EXPECT_EQ(std::u16string(), second_password_element.Value().Utf16());
}

TEST_F(PasswordGenerationAgentTest, DynamicFormTest) {
  LoadHTMLWithUserGesture(kSigninFormHTML);

  ExecuteJavaScriptForTests(
      "var form = document.createElement('form');"
      "form.action='http://www.random.com';"
      "var username = document.createElement('input');"
      "username.type = 'text';"
      "username.id = 'dynamic_username';"
      "var first_password = document.createElement('input');"
      "first_password.type = 'password';"
      "first_password.id = 'first_password';"
      "first_password.name = 'first_password';"
      "var second_password = document.createElement('input');"
      "second_password.type = 'password';"
      "second_password.id = 'second_password';"
      "second_password.name = 'second_password';"
      "form.appendChild(username);"
      "form.appendChild(first_password);"
      "form.appendChild(second_password);"
      "document.body.appendChild(form);");
  WaitForAutofillDidAssociateFormControl();

  // This needs to come after the DOM has been modified.
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);

  // TODO(gcasto): I'm slightly worried about flakes in this test where
  // didAssociateFormControls() isn't called. If this turns out to be a problem
  // adding a call to OnDynamicFormsSeen(GetMainFrame()) will fix it, though
  // it will weaken the test.
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
}

// Losing focus should not trigger a password generation popup.
TEST_F(PasswordGenerationAgentTest, BlurTest) {
  LoadHTMLWithUserGesture(kDisabledElementAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);

  // Focus on the first password field: password generation popup should show
  // up.
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  // Remove focus from everywhere by clicking an unfocusable element: password
  // generation popup should not show up.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
  EXPECT_TRUE(SimulateElementClick("disabled"));
  fake_pw_client_.Flush();
}

TEST_F(PasswordGenerationAgentTest, ChangePasswordFormDetectionTest) {
  // Verify that generation is shown on correct field after message receiving.
  LoadHTMLWithUserGesture(kPasswordChangeFormHTML);
  ExpectAutomaticGenerationAvailable("password", kNotReported);
  ExpectAutomaticGenerationAvailable("newpassword", kNotReported);
  ExpectAutomaticGenerationAvailable("confirmpassword", kNotReported);

  SetFoundFormEligibleForGeneration(password_generation_,
                                    GetMainFrame()->GetDocument(),
                                    /*new_password_id=*/"newpassword",
                                    /*confirm_password_id=*/"confirmpassword");
  ExpectAutomaticGenerationAvailable("password", kNotReported);
  ExpectAutomaticGenerationAvailable("newpassword", kAvailable);
  ExpectGenerationElementLostFocus("confirmpassword");
}

// These tests are for the right-click menu and it is not applicable to Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordGenerationAgentTest, DesktopContextMenuGenerationInFormTest) {
  LoadHTMLWithUserGesture(kSigninFormHTML);
  WebElement element = GetElementById("password");
  WebInputElement first_password_element = element.To<WebInputElement>();
  EXPECT_TRUE(first_password_element.Value().IsNull());
  SimulateElementRightClick("password");
  SelectGenerationFallbackAndExpect(true);
  EXPECT_TRUE(first_password_element.Value().IsNull());

  // Re-focusing a password field for which manual generation was requested
  // should not re-trigger generation.
  ExpectAutomaticGenerationAvailable("password", kNotReported);
}

TEST_F(PasswordGenerationAgentTest, DesktopContextMenuGenerationNoFormTest) {
  LoadHTMLWithUserGesture(kAccountCreationNoForm);
  SimulateElementRightClick("first_password");
  SelectGenerationFallbackAndExpect(true);
}

TEST_F(PasswordGenerationAgentTest,
       DesktopContextMenuGenerationDoesntSuppressAutomatic) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
  // The browser may show a standard password dropdown with the "Generate"
  // option. In this case manual generation is triggered.
  SelectGenerationFallbackAndExpect(true);

  // Move the focus away to somewhere.
  ExpectGenerationElementLostFocus("address");

  // Moving the focus back should trigger the automatic generation again.
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);
}

TEST_F(PasswordGenerationAgentTest, DesktopContextMenuGenerationNoIds) {
  LoadHTMLWithUserGesture(kAccountCreationNoIds);
  WebDocument document = GetMainFrame()->GetDocument();

  ExecuteJavaScriptForTests(
      "document.getElementsByClassName('first_password')[0].focus();");
  WebInputElement first_password_element =
      document.FocusedElement().To<WebInputElement>();
  ASSERT_FALSE(first_password_element.IsNull());
  SelectGenerationFallbackAndExpect(true);

  // Simulate that the user accepts a generated password.
  std::u16string password = u"random_password";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);

  // Check that the first password field is autofilled with the generated
  // password.
  EXPECT_EQ(password, first_password_element.Value().Utf16());
  EXPECT_TRUE(first_password_element.IsAutofilled());

  // Check that the second password field is autofilled with the generated
  // password (since it is chosen as a confirmation password field).
  ExecuteJavaScriptForTests(
      "document.getElementsByClassName('second_password')[0].focus();");
  WebInputElement second_password_element =
      document.FocusedElement().To<WebInputElement>();
  ASSERT_FALSE(second_password_element.IsNull());
  EXPECT_EQ(password, second_password_element.Value().Utf16());
  EXPECT_TRUE(second_password_element.IsAutofilled());
}

TEST_F(PasswordGenerationAgentTest,
       DesktopContextMenuGeneration_NoFocusedElement) {
  // Checks the fallback doesn't cause a crash just in case no password element
  // had focus so far.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SelectGenerationFallbackAndExpect(false);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PasswordGenerationAgentTest, PresavingGeneratedPassword) {
  const struct {
    const char* form;
    const char* generation_element;
  } kTestCases[] = {{kAccountCreationFormHTML, "first_password"},
                    {kAccountCreationNoForm, "first_password"},
                    {kPasswordChangeFormHTML, "newpassword"}};
  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message("form: ") << test_case.form);
    LoadHTMLWithUserGesture(test_case.form);
    FocusField(test_case.generation_element);
    SelectGenerationFallbackAndExpect(true);

    std::u16string password = u"random_password";
    EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
    password_generation_->GeneratedPasswordAccepted(password);
    base::RunLoop().RunUntilIdle();

    EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
    FocusField(test_case.generation_element);
    EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, _));
    SimulateUserTypingASCIICharacter('a', true);
    base::RunLoop().RunUntilIdle();

    EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
    FocusField("username");
    EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, _));
    SimulateUserTypingASCIICharacter('X', true);
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

    EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
    FocusField(test_case.generation_element);
    EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(testing::_));
    for (size_t i = 0; i < password.length(); ++i)
      SimulateUserTypingASCIICharacter(ui::VKEY_BACK, false);
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
    base::RunLoop().RunUntilIdle();

    EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, _)).Times(0);
    EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
    FocusField("username");
    SimulateUserTypingASCIICharacter('Y', true);
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
  }
}

TEST_F(PasswordGenerationAgentTest, FallbackForSaving) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  FocusField("first_password");
  SelectGenerationFallbackAndExpect(true);
  EXPECT_EQ(0, fake_driver_.called_inform_about_user_input_count());
  std::u16string password = u"random_password";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)))
      .WillOnce(testing::InvokeWithoutArgs([this]() {
        // Make sure that generation event was propagated to the browser before
        // the fallback showing. Otherwise, the fallback for saving provides a
        // save bubble instead of a confirmation bubble.
        EXPECT_EQ(0, fake_driver_.called_inform_about_user_input_count());
      }));
  password_generation_->GeneratedPasswordAccepted(password);
  fake_driver_.Flush();
  // Two fallback requests are expected because generation changes either new
  // password and confirmation fields.
  EXPECT_EQ(2, fake_driver_.called_inform_about_user_input_count());
}

TEST_F(PasswordGenerationAgentTest, FormClassifierDisabled) {
  LoadHTMLWithUserGesture(kSigninFormHTML);
  ExpectFormClassifierVoteReceived(false /* vote is not expected */,
                                   std::u16string());
}

TEST_F(PasswordGenerationAgentTest, RevealPassword) {
  // Checks that revealed password is masked when the field lost focus.
  // Test cases: user click on another input field and on non-focusable element.
  LoadHTMLWithUserGesture(kPasswordFormAndSpanHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"password", /*confirm_password_id=*/nullptr);
  const char* kGenerationElementId = "password";
  const char* kSpanId = "span";
  const char* kTextFieldId = "username";

  ExpectAutomaticGenerationAvailable(kGenerationElementId, kAvailable);
  std::u16string password = u"long_pwd";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);

  for (bool clickOnInputField : {false, true}) {
    SCOPED_TRACE(testing::Message("clickOnInputField = ") << clickOnInputField);
    // Click on the generation field to reveal the password value.
    EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
    FocusField(kGenerationElementId);
    fake_pw_client_.Flush();

    WebDocument document = GetMainFrame()->GetDocument();
    WebElement element = GetElementById(kGenerationElementId);
    ASSERT_FALSE(element.IsNull());
    blink::WebInputElement input = element.To<WebInputElement>();
    EXPECT_TRUE(input.ShouldRevealPassword());

    // Click on another HTML element.
    const char* const click_target_name =
        clickOnInputField ? kTextFieldId : kSpanId;
    EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
    EXPECT_TRUE(SimulateElementClick(click_target_name));
    EXPECT_FALSE(input.ShouldRevealPassword());
    fake_pw_client_.Flush();
    testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
  }
}

TEST_F(PasswordGenerationAgentTest, JavascriptClearedTheField) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);

  const char kGenerationElementId[] = "first_password";
  ExpectAutomaticGenerationAvailable(kGenerationElementId, kAvailable);
  std::u16string password = u"long_pwd";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);

  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(testing::_));
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  ExecuteJavaScriptForTests(
      "document.getElementById('first_password').value = '';");
  FocusField(kGenerationElementId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordGenerationAgentTest, GenerationFallback_NoFocusedElement) {
  // Checks the fallback doesn't cause a crash just in case no password element
  // had focus so far.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SelectGenerationFallbackAndExpect(false);
}

TEST_F(PasswordGenerationAgentTest, AutofillToGenerationField) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = GetElementById("first_password");
  const WebInputElement input_element = element.To<WebInputElement>();
  // Since password isn't generated (just suitable field was detected),
  // |OnFieldAutofilled| wouldn't trigger any actions.
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(testing::_)).Times(0);
  password_generation_->OnFieldAutofilled(input_element);
}

TEST_F(PasswordGenerationAgentTestForHtmlAnnotation, AnnotateForm) {
  TestAnnotateForm(true);
}

TEST_F(PasswordGenerationAgentTestForHtmlAnnotation, AnnotateNoForm) {
  TestAnnotateForm(false);
}

TEST_F(PasswordGenerationAgentTest, PasswordUnmaskedUntilCompleteDeletion) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      /*new_password_id=*/"first_password", /*confirm_password_id=*/nullptr);

  constexpr char kGenerationElementId[] = "first_password";

  // Generate a new password.
  ExpectAutomaticGenerationAvailable(kGenerationElementId, kAvailable);
  std::u16string password = u"random_password";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Delete characters of the generated password until only
  // |kMinimumLengthForEditedPassword| - 1 chars remain.
  EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
  FocusField(kGenerationElementId);
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(testing::_));
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  size_t max_chars_to_delete =
      password.length() -
      PasswordGenerationAgent::kMinimumLengthForEditedPassword + 1;
  for (size_t i = 0; i < max_chars_to_delete; ++i)
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, false);
  base::RunLoop().RunUntilIdle();
  fake_pw_client_.Flush();
  // The remaining characters no longer count as a generated password, so
  // generation should be offered again.

  // Check that the characters remain unmasked.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = GetElementById(kGenerationElementId);
  ASSERT_FALSE(element.IsNull());
  blink::WebInputElement input = element.To<WebInputElement>();
  EXPECT_TRUE(input.ShouldRevealPassword());

  // Delete the rest of the characters. The field should now mask new
  // characters. Due to implementation details it's possible to get pings about
  // password generation available.
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_))
      .Times(AnyNumber());
  for (size_t i = 0;
       i < PasswordGenerationAgent::kMinimumLengthForEditedPassword; ++i)
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(input.ShouldRevealPassword());
}

TEST_F(PasswordGenerationAgentTest, ShortPasswordMaskedAfterChangingFocus) {
  LoadHTMLWithUserGesture(kPasswordFormAndSpanHTML);
  constexpr char kGenerationElementId[] = "password";
  SetFoundFormEligibleForGeneration(password_generation_,
                                    GetMainFrame()->GetDocument(),
                                    /*new_password_id=*/kGenerationElementId,
                                    /*confirm_password_id=*/nullptr);

  // Generate a new password.
  ExpectAutomaticGenerationAvailable(kGenerationElementId, kAvailable);
  std::u16string password = u"random_password";
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Delete characters of the generated password until only
  // |kMinimumLengthForEditedPassword| - 1 chars remain.
  EXPECT_CALL(fake_pw_client_, ShowPasswordEditingPopup(_, _, _, _));
  FocusField(kGenerationElementId);
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(testing::_));
  size_t max_chars_to_delete =
      password.length() -
      PasswordGenerationAgent::kMinimumLengthForEditedPassword + 1;
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  for (size_t i = 0; i < max_chars_to_delete; ++i)
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, false);
  // The remaining characters no longer count as a generated password, so
  // generation should be offered again.
  base::RunLoop().RunUntilIdle();
  fake_pw_client_.Flush();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);

  // Check that the characters remain unmasked.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = GetElementById(kGenerationElementId);
  ASSERT_FALSE(element.IsNull());
  blink::WebInputElement input = element.To<WebInputElement>();
  EXPECT_TRUE(input.ShouldRevealPassword());

  // Focus another element on the page. The password should be masked.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus());
  ASSERT_TRUE(SimulateElementClick("span"));
  EXPECT_FALSE(input.ShouldRevealPassword());

  // Focus the password field again. As the remaining characters are not
  // a generated password, they should remain masked.
  ExpectAutomaticGenerationAvailable(kGenerationElementId, kAvailable);
  EXPECT_FALSE(input.ShouldRevealPassword());
}

TEST_F(PasswordGenerationAgentTest, GenerationAvailableByRendererIds) {
  LoadHTMLWithUserGesture(kMultipleAccountCreationFormHTML);

  constexpr const char* kPasswordElementsIds[] = {"password", "first_password",
                                                  "second_password"};

  WebDocument document = GetMainFrame()->GetDocument();
  std::vector<WebInputElement> password_elements;
  for (const char* id : kPasswordElementsIds) {
    WebElement element = GetElementById(id);
    WebInputElement input = element.To<WebInputElement>();
    password_elements.push_back(input);
  }

  // Simulate that the browser informs about eligible for generation form.
  // Check that generation is available only on new password field of this form.
  PasswordFormGenerationData generation_data;
  generation_data.new_password_renderer_id = autofill::FieldRendererId(
      password_elements[0].UniqueRendererFormControlId());

  password_generation_->FoundFormEligibleForGeneration(generation_data);
  ExpectAutomaticGenerationAvailable(kPasswordElementsIds[0], kAvailable);
  ExpectGenerationElementLostFocus(kPasswordElementsIds[1]);
  ExpectAutomaticGenerationAvailable(kPasswordElementsIds[2], kNotReported);

  // Simulate that the browser informs about the second eligible for generation
  // form. Check that generation is available on both forms.
  generation_data.new_password_renderer_id = autofill::FieldRendererId(
      password_elements[2].UniqueRendererFormControlId());
  password_generation_->FoundFormEligibleForGeneration(generation_data);
  ExpectAutomaticGenerationAvailable(kPasswordElementsIds[0], kAvailable);
  ExpectGenerationElementLostFocus(kPasswordElementsIds[1]);
  ExpectAutomaticGenerationAvailable(kPasswordElementsIds[2], kAvailable);
}

TEST_F(PasswordGenerationAgentTest, SuggestionPreviewTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordGenerationPreviewOnHover);

  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  WebDocument document = GetMainFrame()->GetDocument();
  SetFoundFormEligibleForGeneration(password_generation_,
                                    GetMainFrame()->GetDocument(),
                                    "first_password" /* new_passwod_id */,
                                    "second_password" /* confirm_password_id*/);
  ExpectAutomaticGenerationAvailable("first_password", kAvailable);

  WebElement element =
      document.GetElementById(WebString::FromUTF8("first_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement first_password_element = element.To<WebInputElement>();
  element = document.GetElementById(WebString::FromUTF8("second_password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement second_password_element = element.To<WebInputElement>();

  std::u16string password = u"random_password";
  password_generation_->PreviewGenerationSuggestion(password);

  // Both password fields should have sugegsted values.
  EXPECT_EQ(password, first_password_element.SuggestedValue().Utf16());
  EXPECT_EQ(password, second_password_element.SuggestedValue().Utf16());
  EXPECT_EQ(first_password_element.GetAutofillState(),
            blink::WebAutofillState::kPreviewed);
  EXPECT_EQ(second_password_element.GetAutofillState(),
            blink::WebAutofillState::kPreviewed);

  // Previewed suggestions should be successfully cleared upon request.
  EXPECT_TRUE(password_generation_->DidClearGenerationSuggestion(
      first_password_element));
  EXPECT_TRUE(first_password_element.SuggestedValue().IsNull());
  EXPECT_TRUE(second_password_element.SuggestedValue().IsNull());
  EXPECT_EQ(first_password_element.GetAutofillState(),
            blink::WebAutofillState::kNotFilled);
  EXPECT_EQ(second_password_element.GetAutofillState(),
            blink::WebAutofillState::kNotFilled);

  // Clearing should not succeed when there is nothing to clear.
  EXPECT_FALSE(password_generation_->DidClearGenerationSuggestion(
      first_password_element));
}

}  // namespace autofill
