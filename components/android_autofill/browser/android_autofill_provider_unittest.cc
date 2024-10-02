// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_provider.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/android_autofill_features.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/android_autofill_provider_bridge.h"
#include "components/android_autofill/browser/android_autofill_provider_test_api.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/form_data_android_test_api.h"
#include "components/android_autofill/browser/mock_form_field_data_android_bridge.h"
#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/webauthn/android/mock_webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory_test_api.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;
using FieldInfo = AndroidAutofillProviderBridge::FieldInfo;
using PrefillRequestState = AndroidAutofillProvider::PrefillRequestState;
using test::CreateFormDataForFrame;
using test::CreateTestCreditCardFormData;
using test::CreateTestFormField;
using test::CreateTestPasswordFormData;
using test::CreateTestPersonalInformationFormData;

auto EqualsFieldInfo(size_t index) {
  return Field("index", &AndroidAutofillProviderBridge::FieldInfo::index,
               Eq(index));
}

// Creates a matcher that compares a `FormDataAndroid::form()` to `expected`.
auto EqualsFormData(const FormData& expected) {
  return ResultOf(
      [expected](const FormDataAndroid& actual) {
        return FormData::DeepEqual(expected, actual.form());
      },
      true);
}

auto EqualsFormDataWithFields(const FormData& form, auto fields_matcher) {
  return AllOf(
      EqualsFormData(form),
      ResultOf(
          [](FormDataAndroid& form_android)
              -> const std::vector<std::unique_ptr<FormFieldDataAndroid>>& {
            return test_api(form_android).fields();
          },
          fields_matcher));
}

// Creates a matcher that compares the results of a `FormDataAndroid`'s `form()`
// and `session_id()` methods to `form` and `session_id_matcher`.
auto EqualsFormDataWithSessionId(const FormData& form,
                                 auto session_id_matcher) {
  return AllOf(EqualsFormData(form),
               Property(&FormDataAndroid::session_id, session_id_matcher));
}

// Returns an action that writes the `SessionId` of a `FormDataAndroid` into the
// out parameter `session_id`. Note that `session_id` must be valid at least
// until the action is executed.
auto SaveSessionId(SessionId* session_id) {
  return [session_id](const FormDataAndroid& form_android) {
    *session_id = form_android.session_id();
  };
}

FormData CreateTestBasicForm() {
  FormData form;
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_url(GURL("https://foo.com/form.html"));
  form.set_action(GURL("https://foo.com/submit.html"));
  form.set_main_frame_origin(url::Origin::Create(form.url()));
  return form;
}

FormData CreateTestLoginForm() {
  FormData form = CreateTestBasicForm();
  form.set_name(u"login_form");
  form.set_fields(
      {CreateTestFormField(/*label=*/"Username", /*name=*/"username",
                           /*value=*/"", FormControlType::kInputText),
       CreateTestFormField(/*label=*/"Password", /*name=*/"password",
                           /*value=*/"", FormControlType::kInputPassword)});
  return form;
}

FormData CreateTestChangePasswordForm() {
  FormData form = CreateTestBasicForm();
  form.set_name(u"change_password_form");
  form.set_fields(
      {CreateTestFormField(/*label=*/"Password", /*name=*/"password1",
                           /*value=*/"", FormControlType::kInputPassword),
       CreateTestFormField(/*label=*/"Password", /*name=*/"password2",
                           /*value=*/"", FormControlType::kInputPassword)});
  return form;
}

FormData CreateTestWebAuthnPasswordFormData() {
  std::vector<FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", FormControlType::kInputText, /*autocomplete=*/"webauthn"));
  fields.push_back(
      CreateTestFormField(/*label=*/"Password:", /*name=*/"password",
                          /*value=*/"", FormControlType::kInputPassword));
  FormData form;
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_url(GURL("https://foo.com/form.html"));
  form.set_action(GURL("https://foo.com/submit.html"));
  form.set_main_frame_origin(url::Origin::Create(form.url()));
  form.set_fields(std::move(fields));
  return form;
}

class TestAndroidAutofillManager : public AndroidAutofillManager {
 public:
  explicit TestAndroidAutofillManager(ContentAutofillDriver* driver)
      : AndroidAutofillManager(driver) {}

  void OnFormsSeen(const std::vector<FormData>& updated_forms,
                   const std::vector<FormGlobalId>& removed_forms) override {
    TestAutofillManagerWaiter waiter(*this, {AutofillManagerEvent::kFormsSeen});
    AutofillManager::OnFormsSeen(updated_forms, removed_forms);
    ASSERT_TRUE(waiter.Wait());
  }

  void SimulatePropagateAutofillPredictions(FormGlobalId form_id) {
    NotifyObservers(&Observer::OnFieldTypesDetermined, form_id,
                    Observer::FieldTypeSource::kAutofillServer);
  }

  void SimulateOnAskForValuesToFill(const FormData& form,
                                    const FormFieldData& field) {
    gfx::PointF p = field.bounds().origin();
    gfx::Rect caret_bounds(gfx::Point(p.x(), p.y()), gfx::Size(0, 10));
    OnAskForValuesToFillImpl(
        form, field.global_id(), caret_bounds,
        AutofillSuggestionTriggerSource::kTextFieldDidChange);
  }

  void SimulateOnFocusOnFormField(const FormData& form,
                                  const FormFieldData& field) {
    OnFocusOnFormFieldImpl(form, field.global_id());
  }

  void SimulateOnFormSubmitted(const FormData& form,
                               bool known_success,
                               mojom::SubmissionSource source) {
    OnFormSubmittedImpl(form, known_success, source);
  }

  void SimulateOnTextFieldDidChange(const FormData& form,
                                    const FormFieldData& field) {
    OnTextFieldDidChangeImpl(form, field.global_id(), base::TimeTicks::Now());
  }

  void SimulateOnTextFieldDidScroll(const FormData& form,
                                    const FormFieldData& field) {
    OnTextFieldDidScrollImpl(form, field.global_id());
  }
};

class MockAndroidAutofillProviderBridge : public AndroidAutofillProviderBridge {
 public:
  explicit MockAndroidAutofillProviderBridge() = default;

  ~MockAndroidAutofillProviderBridge() override = default;

  MOCK_METHOD(void,
              AttachToJavaAutofillProvider,
              (JNIEnv*, const base::android::JavaRef<jobject>&),
              (override));
  MOCK_METHOD(void, SendPrefillRequest, (FormDataAndroid&), (override));
  MOCK_METHOD(void,
              StartAutofillSession,
              (FormDataAndroid&, const FieldInfo&, bool),
              (override));
  MOCK_METHOD(void, OnServerPredictionsAvailable, (), (override));
  MOCK_METHOD(void,
              ShowDatalistPopup,
              (base::span<const SelectOption>, bool),
              (override));
  MOCK_METHOD(void, HideDatalistPopup, (), (override));
  MOCK_METHOD(void,
              OnFocusChanged,
              (const std::optional<FieldInfo>&),
              (override));
  MOCK_METHOD(void, OnFormFieldDidChange, (const FieldInfo&), (override));
  MOCK_METHOD(void,
              OnFormFieldVisibilitiesDidChange,
              (base::span<const int>),
              (override));
  MOCK_METHOD(void, OnTextFieldDidScroll, (const FieldInfo&), (override));
  MOCK_METHOD(void, OnFormSubmitted, (mojom::SubmissionSource), (override));
  MOCK_METHOD(void, OnDidFillAutofillFormData, (), (override));
  MOCK_METHOD(void, CancelSession, (), (override));
  MOCK_METHOD(void, Reset, (), (override));
};

content::RenderFrameHost* NavigateAndCommitFrame(content::RenderFrameHost* rfh,
                                                 const GURL& url) {
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

class AndroidAutofillProviderTestBase
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Set up mock bridges.
    AndroidAutofillBridgeFactory::GetInstance()
        .SetFormFieldDataAndroidTestingFactory(base::BindLambdaForTesting(
            []() -> std::unique_ptr<FormFieldDataAndroidBridge> {
              return std::make_unique<
                  NiceMock<MockFormFieldDataAndroidBridge>>();
            }));
    AndroidAutofillBridgeFactory::GetInstance()
        .SetAndroidAutofillProviderTestingFactory(base::BindLambdaForTesting(
            [&bridge_ptr = provider_bridge_](
                AndroidAutofillProviderBridge::Delegate* delegate)
                -> std::unique_ptr<AndroidAutofillProviderBridge> {
              auto bridge = std::make_unique<
                  NiceMock<MockAndroidAutofillProviderBridge>>();
              bridge_ptr = bridge.get();
              return bridge;
            }));

    // Create the provider.
    AndroidAutofillProvider::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    provider_bridge_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestAndroidAutofillManager& android_autofill_manager(
      content::RenderFrameHost* rfh = nullptr) {
    return *autofill_manager_injector_[rfh ? rfh : main_frame()];
  }

  AndroidAutofillProvider& autofill_provider() {
    return *AndroidAutofillProvider::FromWebContents(web_contents());
  }

  AndroidAutofillProviderBridge::Delegate& provider_bridge_delegate() {
    return static_cast<AndroidAutofillProviderBridge::Delegate&>(
        autofill_provider());
  }

  // Returns the local frame token of the primary main frame.
  LocalFrameToken main_frame_token() {
    return LocalFrameToken(main_frame()->GetFrameToken().value());
  }

  MockAndroidAutofillProviderBridge& provider_bridge() {
    return *provider_bridge_;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestAndroidAutofillManager>
      autofill_manager_injector_;
  raw_ptr<MockAndroidAutofillProviderBridge> provider_bridge_ = nullptr;
};

class AndroidAutofillProviderTest : public AndroidAutofillProviderTestBase {
 public:
  void SetUp() override {
    AndroidAutofillProviderTestBase::SetUp();

    // Navigation forces the creation of an AndroidAutofillManager for the main
    // frame.
    NavigateAndCommit(GURL("about:blank"));
    FocusWebContentsOnMainFrame();
  }
};

// Tests that AndroidAutofillManager keeps track of the predictions it is
// informed about.
TEST_F(AndroidAutofillProviderTest, HasServerPrediction) {
  FormData form = CreateTestPersonalInformationFormData();
  EXPECT_FALSE(
      android_autofill_manager().has_server_prediction(form.global_id()));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
  EXPECT_TRUE(
      android_autofill_manager().has_server_prediction(form.global_id()));

  // Resetting removes prediction state.
  test_api(android_autofill_manager()).Reset();
  EXPECT_FALSE(
      android_autofill_manager().has_server_prediction(form.global_id()));
}

// Tests that triggering `OnAskForValuesToFill` results in starting an Autofill
// session for the focused form and field.
TEST_F(AndroidAutofillProviderTest, OnAskForValuesToFillStartsSession) {
  base::HistogramTester histogram_tester;

  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  EXPECT_CALL(
      provider_bridge(),
      StartAutofillSession(EqualsFormData(form), EqualsFieldInfo(/*index=*/0),
                           /*has_server_predictions=*/false));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form, form.fields().front());
}

// Tests that a focus change within the form of an ongoing autofill session
// results in a focus change event that is sent to Java.
TEST_F(AndroidAutofillProviderTest, OnFocusChangeInsideCurrentAutofillForm) {
  base::HistogramTester histogram_tester;

  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form, form.fields().front());

  MockFunction<void(int)> check;
  {
    InSequence s;
    EXPECT_CALL(provider_bridge(),
                OnFocusChanged(Optional(EqualsFieldInfo(/*index=*/1))));
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(provider_bridge(), OnFocusChanged(Eq(std::nullopt)));
    EXPECT_CALL(check, Call(2));
  }

  android_autofill_manager().SimulateOnFocusOnFormField(form, form.fields()[1]);
  check.Call(1);
  android_autofill_manager().OnFocusOnNonFormFieldImpl();
  check.Call(2);
}

// Tests that triggering `OnAskForValuesToFill` with a field results in an
// update to last_focused_field_id. The update is important so that
// `AutofillProvider::RendererShouldAcceptDatalistSuggestion` is passed the
// correct field ID.
TEST_F(AndroidAutofillProviderTest, OnAskForValuesToFillFindsCorrectFieldId) {
  base::HistogramTester histogram_tester;

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[0]);

  EXPECT_EQ(test_api(autofill_provider()).last_focused_field_id(),
            form.fields()[0].global_id());

  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[1]);

  EXPECT_EQ(test_api(autofill_provider()).last_focused_field_id(),
            form.fields()[1].global_id());
}

// Tests that Java is informed about visibility changes of form fields connected
// to the current Autofill session if they are detected in focus change events.
TEST_F(AndroidAutofillProviderTest, NotifyAboutVisibilityChangeOnFocus) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  // For Android Autofill, focusability is the same as visibility.
  test_api(form).field(0).set_is_focusable(false);
  test_api(form).field(2).set_is_focusable(false);

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[1]);

  test_api(form).field(0).set_is_focusable(true);
  test_api(form).field(2).set_is_focusable(true);

  EXPECT_CALL(provider_bridge(), OnFormFieldVisibilitiesDidChange(
                                     /*indices=*/UnorderedElementsAre(0, 2)));
  EXPECT_CALL(provider_bridge(),
              OnFocusChanged(Optional(EqualsFieldInfo(/*index=*/0))));
  android_autofill_manager().SimulateOnFocusOnFormField(form, form.fields()[0]);
}

// Tests that asking for values to fill for a different form than that of the
// current Autofill session results in a restart of the session.
TEST_F(AndroidAutofillProviderTest, OnAskForValuesToFillOnOtherForm) {
  base::HistogramTester histogram_tester;

  FormData form1 = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  FormData form2 = CreateFormDataForFrame(
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true),
      main_frame_token());
  android_autofill_manager().OnFormsSeen({form1, form2}, /*removed_forms=*/{});

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(provider_bridge(),
                StartAutofillSession(EqualsFormData(form1),
                                     EqualsFieldInfo(/*index=*/1),
                                     /*has_server_predictions=*/false));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(provider_bridge(),
                StartAutofillSession(EqualsFormData(form2),
                                     EqualsFieldInfo(/*index=*/0),
                                     /*has_server_predictions=*/false));
    EXPECT_CALL(check, Call);
  }

  android_autofill_manager().SimulateOnAskForValuesToFill(form1,
                                                          form1.fields()[1]);
  check.Call();
  android_autofill_manager().SimulateOnAskForValuesToFill(form2,
                                                          form2.fields()[0]);
  check.Call();
}

// Tests that asking for values to fill on the same form as that of the current
// Autofill session results in a restart of the session if the form has changed.
TEST_F(AndroidAutofillProviderTest, OnAskForValuesToFillOnChangedForm) {
  base::HistogramTester histogram_tester;

  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  form.set_name_attribute(u"old_name");
  FormData form_changed = form;
  form_changed.set_name_attribute(u"changed_name");
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(
        provider_bridge(),
        StartAutofillSession(EqualsFormData(form), EqualsFieldInfo(/*index=*/1),
                             /*has_server_predictions=*/false));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(provider_bridge(),
                StartAutofillSession(EqualsFormData(form_changed),
                                     EqualsFieldInfo(/*index=*/1),
                                     /*has_server_predictions=*/false));
    EXPECT_CALL(check, Call);
  }

  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[1]);
  check.Call();
  android_autofill_manager().OnFormsSeen({form_changed}, /*removed_forms=*/{});
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form_changed, form_changed.fields()[1]);
  check.Call();
}

// Tests that asking for values to fill on the same form as that of the current
// Autofill session does not result in a restart of the session if the form
// has not changed.
TEST_F(AndroidAutofillProviderTest, OnAskForValuesToFillOnSameForm) {
  base::HistogramTester histogram_tester;

  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(
        provider_bridge(),
        StartAutofillSession(EqualsFormData(form), EqualsFieldInfo(/*index=*/1),
                             /*has_server_predictions=*/false));
    EXPECT_CALL(check, Call);
  }

  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[1]);
  check.Call();
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[0]);
}

// Tests that value changes in the form of the Autofill session are propagated
// to Java and to the state that `AndroidAutofillProvider` keeps.
TEST_F(AndroidAutofillProviderTest, OnTextFieldDidChange) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[1]);

  // Simulate a value change.
  EXPECT_CALL(provider_bridge(),
              OnFormFieldDidChange(EqualsFieldInfo(/*index=*/1)));
  test_api(form).field(1).set_value(form.fields()[1].value() + u"x");
  android_autofill_manager().SimulateOnTextFieldDidChange(form,
                                                          form.fields()[1]);
  // The `FormDataAndroid` object owned by the provider is also updated.
  ASSERT_TRUE(test_api(autofill_provider()).form());
  EXPECT_EQ(test_api(autofill_provider()).form()->form().fields()[1].value(),
            form.fields()[1].value());
}

// Tests that value changes in a form that is not part of the current Autofill
// session are ignored.
TEST_F(AndroidAutofillProviderTest, OnTextFieldDidChangeInUnrelatedForm) {
  FormData form1 = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  FormData form2 = CreateFormDataForFrame(
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true),
      main_frame_token());
  android_autofill_manager().OnFormsSeen({form1, form2}, /*removed_forms=*/{});

  // Start the Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form1,
                                                          form1.fields()[1]);

  // Simulate a value change in a different form.
  EXPECT_CALL(provider_bridge(), OnFormFieldDidChange).Times(0);
  test_api(form2).field(1).set_value(form2.fields()[1].value() + u"x");
  android_autofill_manager().SimulateOnTextFieldDidChange(form2,
                                                          form2.fields()[1]);
}

// Tests that scrolling events in the form of the Autofill session are
// propagated to Java.
TEST_F(AndroidAutofillProviderTest, OnTextFieldDidScroll) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start the Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[2]);

  // Simulate scrolling.
  EXPECT_CALL(provider_bridge(),
              OnTextFieldDidScroll(EqualsFieldInfo(/*index=*/2)));
  android_autofill_manager().SimulateOnTextFieldDidScroll(form,
                                                          form.fields()[2]);
}

// Tests that scrolling envets in a form that is not part of the current
// Autofill session are ignored.
TEST_F(AndroidAutofillProviderTest, OnTextFieldDidScrollInUnrelatedForm) {
  FormData form1 = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  FormData form2 = CreateFormDataForFrame(
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true),
      main_frame_token());
  android_autofill_manager().OnFormsSeen({form1, form2}, /*removed_forms=*/{});

  // Start the Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form1,
                                                          form1.fields()[1]);

  // Simulate a scroll event in a different form.
  EXPECT_CALL(provider_bridge(), OnFormFieldDidChange).Times(0);
  android_autofill_manager().SimulateOnTextFieldDidScroll(form2,
                                                          form2.fields()[1]);
}

// Tests that a form submission of an ongoing Autofill session is propagated to
// Java if `known_success` is true.
TEST_F(AndroidAutofillProviderTest, OnFormSubmittedWithKnownSuccess) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[0]);

  EXPECT_CALL(provider_bridge(),
              OnFormSubmitted(mojom::SubmissionSource::FORM_SUBMISSION));
  android_autofill_manager().SimulateOnFormSubmitted(
      form, /*known_success=*/true, mojom::SubmissionSource::FORM_SUBMISSION);
}

// Tests that a form submission of an ongoing Autofill session with source
// DOM_MUTATION_AFTER_AUTOFILL is propagated to Java for password forms.
TEST_F(AndroidAutofillProviderTest,
       DomMutationAfterAutofillFormSubmission_PasswordForm) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillAcceptDomMutationAfterAutofillSubmission};
  FormData form =
      CreateFormDataForFrame(CreateTestPasswordFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[0]);

  EXPECT_CALL(
      provider_bridge(),
      OnFormSubmitted(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL))
      .Times(1);
  android_autofill_manager().SimulateOnFormSubmitted(
      form, /*known_success=*/true,
      mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
}

// Tests that a form submission of an ongoing Autofill session with source
// DOM_MUTATION_AFTER_AUTOFILL is not propagated to Java for non-password forms.
TEST_F(AndroidAutofillProviderTest,
       DomMutationAfterAutofillFormSubmission_NonPasswordForm) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillAcceptDomMutationAfterAutofillSubmission};
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[0]);

  EXPECT_CALL(
      provider_bridge(),
      OnFormSubmitted(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL))
      .Times(0);
  android_autofill_manager().SimulateOnFormSubmitted(
      form, /*known_success=*/true,
      mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
}

// Tests that a form submission of an ongoing Autofill session is propagated to
// Java when the `AutofillManager` of the tab is reset, even if the form
// submission was not known to be a success.
TEST_F(AndroidAutofillProviderTest, FormSubmissionHappensOnReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidAutofillDirectFormSubmission);
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[0]);

  EXPECT_CALL(provider_bridge(), OnFormSubmitted).Times(0);
  android_autofill_manager().SimulateOnFormSubmitted(
      form, /*known_success=*/false,
      mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
  Mock::VerifyAndClearExpectations(&provider_bridge());

  EXPECT_CALL(
      provider_bridge(),
      OnFormSubmitted(mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED));
  test_api(android_autofill_manager()).Reset();
}

// Tests that a form submission of an ongoing Autofill session is propagated to
// Java directly on submission, even if the form submission was not known to be
// a success.
TEST_F(AndroidAutofillProviderTest, FormSubmissionHappensDirectly) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAndroidAutofillDirectFormSubmission};
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields()[0]);

  EXPECT_CALL(
      provider_bridge(),
      OnFormSubmitted(mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED));
  android_autofill_manager().SimulateOnFormSubmitted(
      form, /*known_success=*/false,
      mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
}

// Tests the predictions from `password_manager::FormDataParser` are used to
// overwrite all type predictions of the respective `FormDataAndroidField`s.
TEST_F(AndroidAutofillProviderTest,
       UsePasswordManagerOverridesInPrefillRequest) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  auto has_field_type = [](FieldType field_type) {
    return Pointee(Property(&FormFieldDataAndroid::field_types,
                            Eq(AutofillType(field_type))));
  };
  EXPECT_CALL(provider_bridge(),
              SendPrefillRequest(EqualsFormDataWithFields(
                  form, ElementsAre(has_field_type(FieldType::USERNAME),
                                    has_field_type(FieldType::PASSWORD)))));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
}

// Tests that the session id used in a prefill request is also used for starting
// the Autofill session even if the forms are not similar as long as their form
// signatures (and predictions) match.
TEST_F(AndroidAutofillProviderTest,
       SessionIdIsReusedForCachedFormsAsLongAsPredictionsAgree) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));
  FormData changed_form = form;
  changed_form.set_name_attribute(changed_form.name_attribute() +
                                  u"some-suffix");

  SessionId cache_session_id = SessionId(0);
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(form)))
        .WillOnce(SaveSessionId(&cache_session_id));
    EXPECT_CALL(check, Call);
    // Pass a lambda to perform the check because passing `cache_session_id`
    // would match against the current value of cache_session_id (0).
    EXPECT_CALL(provider_bridge(),
                StartAutofillSession(
                    EqualsFormDataWithSessionId(
                        changed_form, Truly([&cache_session_id](SessionId id) {
                          return id == cache_session_id;
                        })),
                    EqualsFieldInfo(/*index=*/0),
                    /*has_server_predictions=*/true));
  }

  // Upon receiving server predictions a prefill request should be sent.
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
  check.Call();

  // The changed form has the same signature as the cached form - therefore it
  // should have the session id of the cached form.
  ASSERT_EQ(CalculateFormSignature(form), CalculateFormSignature(changed_form));
  android_autofill_manager().OnFormsSeen({changed_form}, /*removed_forms=*/{});
  android_autofill_manager().SimulateOnAskForValuesToFill(
      changed_form, changed_form.fields().front());
}

// Tests that new document navigation (manager reset) cancels the ongoing
// autofill session
TEST_F(AndroidAutofillProviderTest, CancelSessionOnNavigation) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  EXPECT_CALL(
      provider_bridge(),
      StartAutofillSession(EqualsFormData(form), EqualsFieldInfo(/*index=*/0),
                           /*has_server_predictions=*/false));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form, form.fields().front());

  EXPECT_CALL(provider_bridge(), CancelSession());
  test_api(android_autofill_manager()).Reset();
}

class AndroidAutofillProviderWithCredManTest
    : public AndroidAutofillProviderTestBase {
 public:
  void SetUp() override {
    AndroidAutofillProviderTestBase::SetUp();

    autofill_provider().MaybeInitKeyboardSuppressor();

    // Navigation creates the AndroidAutofillManager for the main frame.
    NavigateAndCommit(GURL("about:blank"));
    FocusWebContentsOnMainFrame();

    // Load a form with webuthn-annotated username and regular password fields.
    test_webauthn_form_ = CreateFormDataForFrame(
        CreateTestWebAuthnPasswordFormData(), main_frame_token());

    InitializeWebAuthnFactoryWithMock();
  }

  void InitializeWebAuthnFactoryWithMock() {
    auto mock_cred_man_delegate =
        std::make_unique<NiceMock<webauthn::MockWebAuthnCredManDelegate>>();
    webauthn::test_api(web_authn_delegate_factory())
        .EmplaceDelegateForFrame(main_frame(),
                                 std::move(mock_cred_man_delegate));

    ON_CALL(cred_man_delegate(), HasPasskeys())
        .WillByDefault(
            Return(webauthn::WebAuthnCredManDelegate::State::kHasPasskeys));
  }

  webauthn::WebAuthnCredManDelegateFactory* web_authn_delegate_factory() {
    return webauthn::WebAuthnCredManDelegateFactory::GetFactory(web_contents());
  }

  const FormData& test_form() const { return test_webauthn_form_; }

  const FormFieldData& webauthn_email_field() const {
    return test_form().fields()[0];
  }

  const FormFieldData& non_webauthn_password_field() const {
    return test_form().fields()[1];
  }

  void FocusFormField(const FormFieldData& field) {
    keyboard_suppressor().OnBeforeAskForValuesToFill(
        android_autofill_manager(), test_webauthn_form_.global_id(),
        field.global_id(), test_webauthn_form_);
    android_autofill_manager().SimulateOnAskForValuesToFill(test_webauthn_form_,
                                                            field);
    android_autofill_manager().SimulateOnFocusOnFormField(test_webauthn_form_,
                                                          field);
    keyboard_suppressor().OnAfterAskForValuesToFill(
        android_autofill_manager(), test_webauthn_form_.global_id(),
        field.global_id());
  }

  webauthn::MockWebAuthnCredManDelegate& cred_man_delegate() {
    return *static_cast<webauthn::MockWebAuthnCredManDelegate*>(
        web_authn_delegate_factory()->GetRequestDelegate(main_frame()));
  }

  TouchToFillKeyboardSuppressor& keyboard_suppressor() {
    return test_api(autofill_provider()).keyboard_suppressor();
  }

 private:
  FormData test_webauthn_form_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillVirtualViewStructureAndroid};
};

TEST_F(AndroidAutofillProviderWithCredManTest, CallsCredManOnlyOnce) {
  EXPECT_CALL(cred_man_delegate(),
              TriggerCredManUi(Eq(
                  webauthn::WebAuthnCredManDelegate::RequestPasswords(false))));
  FocusFormField(webauthn_email_field());

  // No further call!
  FocusFormField(non_webauthn_password_field());
  FocusFormField(webauthn_email_field());
}

TEST_F(AndroidAutofillProviderWithCredManTest,
       CallsCredManAgainAfterNavigation) {
  EXPECT_CALL(cred_man_delegate(),
              TriggerCredManUi(Eq(
                  webauthn::WebAuthnCredManDelegate::RequestPasswords(false))));
  FocusFormField(webauthn_email_field());

  // Navigate which allows CredMan to be shown again.
  NavigateAndCommit(GURL("about:to-do-stuff"));
  EXPECT_CALL(cred_man_delegate(),
              TriggerCredManUi(Eq(
                  webauthn::WebAuthnCredManDelegate::RequestPasswords(false))));
  FocusFormField(webauthn_email_field());
}

TEST_F(AndroidAutofillProviderWithCredManTest,
       SuppressesKeyboardForCredManCall) {
  // Since CredMan handles this focus, stop the propagation of the focus event.
  EXPECT_CALL(provider_bridge(), OnFocusChanged).Times(0);
  EXPECT_CALL(cred_man_delegate(),
              TriggerCredManUi(Eq(
                  webauthn::WebAuthnCredManDelegate::RequestPasswords(false))));
  base::RepeatingCallback<void(bool)> completed_callback;
  EXPECT_CALL(cred_man_delegate(), SetRequestCompletionCallback)
      .WillOnce(SaveArg<0>(&completed_callback));
  FocusFormField(webauthn_email_field());

  // Keyboard is suppressed while CredMan is showing.
  EXPECT_TRUE(keyboard_suppressor().is_suppressing());
  Mock::VerifyAndClearExpectations(&cred_man_delegate());

  // After resetting CredMan, the keyboard suppressing stopped.
  completed_callback.Run(/*success=*/true);
  EXPECT_FALSE(keyboard_suppressor().is_suppressing());
}

TEST_F(AndroidAutofillProviderWithCredManTest, NoCredManWithoutAnnotation) {
  EXPECT_CALL(provider_bridge(), OnFocusChanged);
  EXPECT_CALL(cred_man_delegate(), TriggerCredManUi).Times(0);
  FocusFormField(non_webauthn_password_field());

  EXPECT_FALSE(keyboard_suppressor().is_suppressing());
}

TEST_F(AndroidAutofillProviderWithCredManTest, SkipsCredManCallBeforeReady) {
  ON_CALL(cred_man_delegate(), HasPasskeys())
      .WillByDefault(
          Return(webauthn::WebAuthnCredManDelegate::State::kNotReady));

  EXPECT_CALL(cred_man_delegate(), TriggerCredManUi).Times(0);
  FocusFormField(webauthn_email_field());
}

TEST_F(AndroidAutofillProviderWithCredManTest, NotifyFocusOnCredManError) {
  EXPECT_CALL(cred_man_delegate(), TriggerCredManUi);
  base::RepeatingCallback<void(bool)> completed_callback;
  EXPECT_CALL(cred_man_delegate(), SetRequestCompletionCallback)
      .WillOnce(SaveArg<0>(&completed_callback));
  FocusFormField(webauthn_email_field());

  // Since CredMan didn't handles this focus, inform the autofill bridge.
  EXPECT_CALL(provider_bridge(), OnFocusChanged);
  completed_callback.Run(/*success=*/false);
}

using AndroidAutofillProviderPrefillRequestTest = AndroidAutofillProviderTest;

// Tests that we can send another prefill request after navigation.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       MultiplePrefillRequestsOnNavigation) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  FormData form = test::GetFormData(
      {.fields = {{.autocomplete_attribute = "username"},
                  {.autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword}}});

  EXPECT_CALL(provider_bridge(), SendPrefillRequest).Times(2);
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form, form.fields().front());
  test_api(android_autofill_manager()).Reset();
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
}

// Tests that a metric is emitted if prefill requests are supported and there
// was not enough time to send a prefill request.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       OnAskForValuesToFillRecordsPrefillRequestStateUmaMetric) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form, form.fields().front());
  histogram_tester.ExpectUniqueSample(
      AndroidAutofillProvider::kPrefillRequestStateUma,
      PrefillRequestState::kRequestNotSentNoTime, 1);
}

// Tests that no prefill requests are sent on Android versions prior to U even
// if all other requirements are satisfied.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       NoPrefillRequestOnVersionsPriorToU) {
  // This test only makes sense on Android versions smaller than U.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  // No prefill request is ever sent.
  EXPECT_CALL(provider_bridge(), SendPrefillRequest).Times(0);
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
}

// Tests that a prefill request is sent if all requirements for it are
// satisfied.
TEST_F(AndroidAutofillProviderPrefillRequestTest, SendPrefillRequest) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  // Upon receiving server predictions a prefill request should be sent.
  EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(form)));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
}

// Tests that no prefill request is sent if there is already an ongoing Autofill
// session.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       NoPrefillRequestIfOngoingSession) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  FormData login_form1 =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form1}, /*removed_forms=*/{});
  EXPECT_CALL(provider_bridge(), StartAutofillSession);
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form1, login_form1.fields().front());
  histogram_tester.ExpectUniqueSample(
      AndroidAutofillProvider::kPrefillRequestStateUma,
      PrefillRequestState::kRequestNotSentNoTime, 1);

  FormData login_form2 =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form2}, /*removed_forms=*/{});
  ASSERT_TRUE(
      android_autofill_manager().FindCachedFormById(login_form2.global_id()));

  // No prefill request is ever sent.
  EXPECT_CALL(provider_bridge(), SendPrefillRequest).Times(0);
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form2.global_id());
}

// Tests that no prefill request is sent if there has already been another
// prefill request.
TEST_F(AndroidAutofillProviderPrefillRequestTest, NoSecondPrefillRequest) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  FormData login_form1 =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form1}, /*removed_forms=*/{});
  ASSERT_TRUE(
      android_autofill_manager().FindCachedFormById(login_form1.global_id()));

  FormData login_form2 =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form2}, /*removed_forms=*/{});
  ASSERT_TRUE(
      android_autofill_manager().FindCachedFormById(login_form2.global_id()));
  // The helper method should generate different ids every time it is called.
  ASSERT_FALSE(FormData::DeepEqual(login_form1, login_form2));

  EXPECT_CALL(provider_bridge(),
              SendPrefillRequest(EqualsFormData(login_form1)));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form1.global_id());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  EXPECT_CALL(provider_bridge(), SendPrefillRequest).Times(0);
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form2.global_id());

  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form2, login_form2.fields().front());
  histogram_tester.ExpectUniqueSample(
      AndroidAutofillProvider::kPrefillRequestStateUma,
      PrefillRequestState::kRequestNotSentMaxNumberReached, 1);
}

// Tests that the session id used in a prefill request is also used for starting
// the Autofill session for that form.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       SessionIdIsReusedForCachedForms) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  // Upon receiving server predictions a prefill request should be sent.
  SessionId cache_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(form)))
      .WillOnce(SaveSessionId(&cache_session_id));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  EXPECT_CALL(
      provider_bridge(),
      StartAutofillSession(EqualsFormDataWithSessionId(form, cache_session_id),
                           EqualsFieldInfo(/*index=*/0),
                           /*has_server_predictions=*/true));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form, form.fields().front());
}

// Tests that the session id used in a prefill request is not reused when
// starting a session on a form with the same id, but changed field content.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       SessionIdIsNotReusedForCachedFormsIfContentHasChanged) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Upon receiving server predictions a prefill request should be sent.
  SessionId cache_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(form)))
      .WillOnce(SaveSessionId(&cache_session_id));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  FormData changed_form = form;
  test_api(changed_form).Remove(-1);
  android_autofill_manager().OnFormsSeen({changed_form},
                                         /*removed_forms=*/{});
  SessionId autofill_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(),
              StartAutofillSession(EqualsFormData(changed_form),
                                   EqualsFieldInfo(/*index=*/0),
                                   /*has_server_predictions=*/true))
      .WillOnce(WithArg<0>(SaveSessionId(&autofill_session_id)));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      changed_form, changed_form.fields().front());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  // A new session id is used to start the Autofill session.
  EXPECT_NE(cache_session_id, autofill_session_id);
  histogram_tester.ExpectUniqueSample(
      AndroidAutofillProvider::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentFormChanged, 1);
}

// Tests that the session id used in a prefill request is only used once to
// start an Autofill session. If the user then focuses on a different form
// before returning to the (formerly) cached form, a new session is started.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       SessionIdIsNotReusedMultipleAutofillSessions) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  FormData pw_form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  FormData pi_form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({pw_form, pi_form},
                                         /*removed_forms=*/{});

  // Upon receiving server predictions a prefill request should be sent.
  SessionId cache_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(pw_form)))
      .WillOnce(SaveSessionId(&cache_session_id));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      pw_form.global_id());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  EXPECT_CALL(provider_bridge(),
              StartAutofillSession(
                  EqualsFormDataWithSessionId(pw_form, cache_session_id),
                  EqualsFieldInfo(/*index=*/0),
                  /*has_server_predictions=*/true));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      pw_form, pw_form.fields().front());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  // Now focus on a different form.
  SessionId pi_form_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(),
              StartAutofillSession(EqualsFormData(pi_form),
                                   EqualsFieldInfo(/*index=*/0),
                                   /*has_server_predictions=*/false))
      .WillOnce(WithArg<0>(SaveSessionId(&pi_form_session_id)));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      pi_form, pi_form.fields().front());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  // Unrelated forms should have different session ids.
  EXPECT_NE(cache_session_id, pi_form_session_id);

  // Focus back on the original password form.
  SessionId pw_form_second_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(),
              StartAutofillSession(EqualsFormData(pw_form),
                                   EqualsFieldInfo(/*index=*/0),
                                   /*has_server_predictions=*/true))
      .WillOnce(WithArg<0>(SaveSessionId(&pw_form_second_session_id)));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      pw_form, pw_form.fields().front());
  Mock::VerifyAndClearExpectations(&provider_bridge());
  // The session id used when focusing back should be different from both those
  // before.
  EXPECT_NE(cache_session_id, pw_form_second_session_id);
  EXPECT_NE(pi_form_session_id, pw_form_second_session_id);
}

// Tests that the prefill request is sent for a Change Password form.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       PrefillRequestSentForChangePasswordForm) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForChangePassword);

  FormData form = CreateFormDataForFrame(CreateTestChangePasswordForm(),
                                         main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(form)));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
}

// Tests that starting an autofill session for a change password form works.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       SessionStartForChangePasswordForm) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForChangePassword);

  FormData form = CreateFormDataForFrame(CreateTestChangePasswordForm(),
                                         main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  // Upon receiving server predictions a prefill request should be sent.
  SessionId cache_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(form)))
      .WillOnce(SaveSessionId(&cache_session_id));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  EXPECT_CALL(
      provider_bridge(),
      StartAutofillSession(EqualsFormDataWithSessionId(form, cache_session_id),
                           EqualsFieldInfo(/*index=*/0),
                           /*has_server_predictions=*/true));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      form, form.fields().front());
}

// Tests that metrics are emitted when the bottom sheet is shown.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       PrefillRequestStateEmittedOnShowingBottomSheet) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  FormData login_form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form.global_id());

  EXPECT_CALL(provider_bridge(), StartAutofillSession);
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form, login_form.fields().front());

  // Simulate a successfully shown bottom sheet.
  provider_bridge_delegate().OnShowBottomSheetResult(
      /*is_shown=*/true, /*provided_autofill_structure=*/true);
  histogram_tester.ExpectUniqueSample(
      AndroidAutofillProvider::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentStructureProvidedBottomSheetShown, 1);
}

// Tests that the correct metrics are emitted when the bottom sheet is not shown
// and no view structure was provided to the Android framework.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       PrefillRequestStateEmittedOnNotShowingBottomSheetWithoutViewStructure) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  FormData login_form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form.global_id());
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form, login_form.fields().front());

  // Simulate a successfully shown bottom sheet.
  provider_bridge_delegate().OnShowBottomSheetResult(
      /*is_shown=*/false, /*provided_autofill_structure=*/false);
  histogram_tester.ExpectUniqueSample(
      AndroidAutofillProvider::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentStructureNotProvided, 1);
  histogram_tester.ExpectTotalCount(
      AndroidAutofillProvider::
          kPrefillRequestBottomsheetNoViewStructureDelayUma,
      1);
}

// Tests that the correct metrics are emitted when the bottom sheet is not shown
// and a view structure was provided to the Android framework.
TEST_F(AndroidAutofillProviderPrefillRequestTest,
       PrefillRequestStateEmittedOnNotShowingBottomSheetWithViewStructure) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  FormData login_form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form.global_id());
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form, login_form.fields().front());

  // Simulate a successfully shown bottom sheet.
  provider_bridge_delegate().OnShowBottomSheetResult(
      /*is_shown=*/false, /*provided_autofill_structure=*/true);
  histogram_tester.ExpectUniqueSample(
      AndroidAutofillProvider::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentStructureProvidedBottomSheetNotShown, 1);
}

class AndroidAutofillProviderTestHidingLogic
    : public AndroidAutofillProviderTest {
 public:
  void SetUp() override {
    AndroidAutofillProviderTest::SetUp();
    NavigateAndCommit(GURL("https://foo.com"));
    sub_frame_ = content::RenderFrameHostTester::For(main_frame())
                     ->AppendChild(std::string("child"));
    sub_frame_ = NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com"));
  }

  void TearDown() override {
    sub_frame_ = nullptr;
    AndroidAutofillProviderTest::TearDown();
  }

  void AskForValuesToFill(content::RenderFrameHost* rfh) {
    FocusWebContentsOnFrame(rfh);
    FormData form =
        CreateFormDataForFrame(CreateTestPersonalInformationFormData(),
                               LocalFrameToken(rfh->GetFrameToken().value()));
    android_autofill_manager(rfh).OnFormsSeen({form},
                                              /*removed_forms=*/{});
    // Start an Autofill session.
    android_autofill_manager(rfh).SimulateOnAskForValuesToFill(
        form, form.fields()[0]);
  }

 protected:
  raw_ptr<content::RenderFrameHost> sub_frame_ = nullptr;
};

// Tests that if the popup is shown in the *main frame*, destruction of the
// *sub frame* does not hide the popup.
TEST_F(AndroidAutofillProviderTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameDestruction) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(0);
  content::RenderFrameHostTester::For(sub_frame_)->Detach();
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&provider_bridge());
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *sub frame* does not hide the popup.
TEST_F(AndroidAutofillProviderTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameNavigation) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(0);
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&provider_bridge());
}

// Tests that if the popup is shown in the *main frame*, destruction of the
// *main frame* resets the java instance which hides the popup.
TEST_F(AndroidAutofillProviderTestHidingLogic, HideInMainFrameOnDestruction) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), Reset);
  // TearDown() destructs the main frame.
}

// Tests that if the popup is shown in the *sub frame*, destruction of the
// *sub frame* hides the popup.
TEST_F(AndroidAutofillProviderTestHidingLogic, HideInSubFrameOnDestruction) {
  AskForValuesToFill(sub_frame_);
  EXPECT_CALL(provider_bridge(), Reset);
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&provider_bridge());
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AndroidAutofillProviderTestHidingLogic,
       HideInMainFrameOnMainFrameNavigation) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *sub frame* hides the popup.
//
// TODO(crbug.com/40283554): Disabled because AndroidAutofillProvider::Reset()
// resets AndroidAutofillProvider::field_rfh_ before RenderFrameDeleted(), which
// prevents OnPopupHidden().
TEST_F(AndroidAutofillProviderTestHidingLogic,
       DISABLED_HideInSubFrameOnSubFrameNavigation) {
  AskForValuesToFill(sub_frame_);
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AndroidAutofillProviderTestHidingLogic,
       HideInSubFrameOnMainFrameNavigation) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

// Tests that AndroidAutofillProvider::last_queried_field_rfh_id_ is updated
// when different frames are queried.
TEST_F(AndroidAutofillProviderTestHidingLogic,
       FollowAskForValuesInDifferentFrames) {
  AskForValuesToFill(main_frame());
  AskForValuesToFill(sub_frame_);
  EXPECT_CALL(provider_bridge(), Reset);
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
}

}  // namespace
}  // namespace autofill
