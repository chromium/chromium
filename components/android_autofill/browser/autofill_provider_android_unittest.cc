// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/android_autofill_features.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/autofill_provider_android.h"
#include "components/android_autofill/browser/autofill_provider_android_bridge.h"
#include "components/android_autofill/browser/autofill_provider_android_test_api.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/mock_form_field_data_android_bridge.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
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
using ::testing::Eq;
using ::testing::Field;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;
using FieldInfo = AutofillProviderAndroidBridge::FieldInfo;
using PrefillRequestState = AutofillProviderAndroid::PrefillRequestState;
using test::CreateFormDataForFrame;
using test::CreateTestCreditCardFormData;
using test::CreateTestFormField;
using test::CreateTestPersonalInformationFormData;

auto EqualsFieldInfo(size_t index) {
  return Field("index", &AutofillProviderAndroidBridge::FieldInfo::index,
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

// Creates a matcher that compares the results of a `FormDataAndroid`'s `form()`
// and `session_id()` methods to `form` and `session_id`.
auto EqualsFormDataWithSessionId(const FormData& form, SessionId session_id) {
  return AllOf(EqualsFormData(form),
               Property(&FormDataAndroid::session_id, session_id));
}

// Returns an action that writes the `SessionId` of a `FormDataAndroid` into the
// out parameter `session_id`. Note that `session_id` must be valid at least
// until the action is executed.
auto SaveSessionId(SessionId* session_id) {
  return [session_id](const FormDataAndroid& form_android) {
    *session_id = form_android.session_id();
  };
}

FormData CreateTestLoginForm() {
  FormData form;
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"login_form";
  form.url = GURL("https://foo.com/form.html");
  form.action = GURL("https://foo.com/submit.html");
  form.main_frame_origin = url::Origin::Create(form.url);
  form.fields = {
      CreateTestFormField(/*label=*/"Username", /*name=*/"username",
                          /*value=*/"", FormControlType::kInputText),
      CreateTestFormField(/*label=*/"Password", /*name=*/"password",
                          /*value=*/"", FormControlType::kInputPassword)};
  return form;
}

class TestAndroidAutofillManager : public AndroidAutofillManager {
 public:
  explicit TestAndroidAutofillManager(ContentAutofillDriver* driver,
                                      ContentAutofillClient* client)
      : AndroidAutofillManager(driver, client) {}

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
    OnAskForValuesToFillImpl(
        form, field, gfx::RectF(),
        AutofillSuggestionTriggerSource::kTextFieldDidChange);
  }

  void SimulateOnFocusOnFormField(const FormData& form,
                                  const FormFieldData& field) {
    OnFocusOnFormFieldImpl(form, field, gfx::RectF());
  }

  void SimulateOnFormSubmitted(const FormData& form,
                               bool known_success,
                               mojom::SubmissionSource source) {
    OnFormSubmittedImpl(form, known_success, source);
  }

  void SimulateOnTextFieldDidChange(const FormData& form,
                                    const FormFieldData& field) {
    OnTextFieldDidChangeImpl(form, field, gfx::RectF(), base::TimeTicks::Now());
  }

  void SimulateOnTextFieldDidScroll(const FormData& form,
                                    const FormFieldData& field) {
    OnTextFieldDidScrollImpl(form, field, gfx::RectF());
  }
};

class MockAutofillProviderAndroidBridge : public AutofillProviderAndroidBridge {
 public:
  explicit MockAutofillProviderAndroidBridge() = default;

  ~MockAutofillProviderAndroidBridge() override = default;

  MOCK_METHOD(void,
              AttachToJavaAutofillProvider,
              (JNIEnv*, const base::android::JavaRef<jobject>&),
              (override));
  MOCK_METHOD(void, SendPrefillRequest, (FormDataAndroid&), (override));
  MOCK_METHOD(void,
              StartAutofillSession,
              (FormDataAndroid&, const FieldInfo&, bool),
              (override));
  MOCK_METHOD(void, OnServerPredictionQueryDone, (bool success), (override));
  MOCK_METHOD(void,
              ShowDatalistPopup,
              (base::span<const SelectOption>, bool),
              (override));
  MOCK_METHOD(void, HideDatalistPopup, (), (override));
  MOCK_METHOD(void,
              OnFocusChanged,
              (const absl::optional<FieldInfo>&),
              (override));
  MOCK_METHOD(void, OnFormFieldDidChange, (const FieldInfo&), (override));
  MOCK_METHOD(void,
              OnFormFieldVisibilitiesDidChange,
              (base::span<const int>),
              (override));
  MOCK_METHOD(void, OnTextFieldDidScroll, (const FieldInfo&), (override));
  MOCK_METHOD(void, OnFormSubmitted, (mojom::SubmissionSource), (override));
  MOCK_METHOD(void, OnDidFillAutofillFormData, (), (override));
  MOCK_METHOD(void, Reset, (), (override));
};

content::RenderFrameHost* NavigateAndCommitFrame(content::RenderFrameHost* rfh,
                                                 const GURL& url) {
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

}  // namespace

class AutofillProviderAndroidTest : public content::RenderViewHostTestHarness {
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
        .SetAutofillProviderAndroidTestingFactory(base::BindLambdaForTesting(
            [&bridge_ptr = provider_bridge_](
                AutofillProviderAndroidBridge::Delegate* delegate)
                -> std::unique_ptr<AutofillProviderAndroidBridge> {
              auto bridge = std::make_unique<
                  NiceMock<MockAutofillProviderAndroidBridge>>();
              bridge_ptr = bridge.get();
              return bridge;
            }));

    // Create the provider.
    AutofillProviderAndroid::CreateForWebContents(web_contents());

    // Navigation forces the creation of an AndroidAutofillManager for the main
    // frame.
    NavigateAndCommit(GURL("about:blank"));
    FocusWebContentsOnMainFrame();
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

  AutofillProviderAndroid& autofill_provider() {
    return *AutofillProviderAndroid::FromWebContents(web_contents());
  }

  AutofillProviderAndroidBridge::Delegate& provider_bridge_delegate() {
    return static_cast<AutofillProviderAndroidBridge::Delegate&>(
        autofill_provider());
  }

  // Returns the local frame token of the primary main frame.
  LocalFrameToken main_frame_token() {
    return LocalFrameToken(main_frame()->GetFrameToken().value());
  }

  MockAutofillProviderAndroidBridge& provider_bridge() {
    return *provider_bridge_;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestAndroidAutofillManager>
      autofill_manager_injector_;
  raw_ptr<MockAutofillProviderAndroidBridge> provider_bridge_ = nullptr;
};

// Tests that AndroidAutofillManager keeps track of the predictions it is
// informed about.
TEST_F(AutofillProviderAndroidTest, HasServerPrediction) {
  FormData form = CreateTestPersonalInformationFormData();
  EXPECT_FALSE(
      android_autofill_manager().has_server_prediction(form.global_id()));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
  EXPECT_TRUE(
      android_autofill_manager().has_server_prediction(form.global_id()));

  // Resetting removes prediction state.
  android_autofill_manager().Reset();
  EXPECT_FALSE(
      android_autofill_manager().has_server_prediction(form.global_id()));
}

// Tests that triggering `OnAskForValuesToFill` results in starting an Autofill
// session for the focused form and field.
TEST_F(AutofillProviderAndroidTest, OnAskForValuesToFillStartsSession) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  EXPECT_CALL(
      provider_bridge(),
      StartAutofillSession(EqualsFormData(form), EqualsFieldInfo(/*index=*/0),
                           /*has_server_predictions=*/false));
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields.front());
}

// Tests that a metric is emitted if prefill requests are supported and there
// was not enough time to send a prefill request.
TEST_F(AutofillProviderAndroidTest,
       OnAskForValuesToFillRecordsPrefillRequestStateUmaMetric) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields.front());
  histogram_tester.ExpectUniqueSample(
      AutofillProviderAndroid::kPrefillRequestStateUma,
      PrefillRequestState::kRequestNotSentNoTime, 1);
}

// Tests that a focus change within the form of an ongoing autofill session
// results in a focus change event that is sent to Java.
TEST_F(AutofillProviderAndroidTest, OnFocusChangeInsideCurrentAutofillForm) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields.front());

  EXPECT_CALL(provider_bridge(),
              OnFocusChanged(Optional(EqualsFieldInfo(/*index=*/1))));
  android_autofill_manager().SimulateOnFocusOnFormField(form, form.fields[1]);

  EXPECT_CALL(provider_bridge(), OnFocusChanged(Eq(absl::nullopt)));
  android_autofill_manager().OnFocusNoLongerOnFormImpl(
      /*had_interacted_form=*/true);
}

// Tests that Java is informed about visibility changes of form fields connected
// to the current Autofill session if they are detected in focus change events.
TEST_F(AutofillProviderAndroidTest, NotifyAboutVisibilityChangeOnFocus) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillSupportVisibilityChanges);

  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  // For Android Autofill, focusability is the same as visibility.
  form.fields[0].is_focusable = false;
  form.fields[2].is_focusable = false;

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form, form.fields[1]);

  form.fields[0].is_focusable = true;
  form.fields[2].is_focusable = true;

  EXPECT_CALL(provider_bridge(), OnFormFieldVisibilitiesDidChange(
                                     /*indices=*/UnorderedElementsAre(0, 2)));
  EXPECT_CALL(provider_bridge(),
              OnFocusChanged(Optional(EqualsFieldInfo(/*index=*/0))));
  android_autofill_manager().SimulateOnFocusOnFormField(form, form.fields[0]);
}

// Tests that asking for values to fill for a different form than that of the
// current Autofill session results in a restart of the session.
TEST_F(AutofillProviderAndroidTest, OnAskForValuesToFillOnOtherForm) {
  FormData form1 = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  FormData form2 = CreateFormDataForFrame(
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true),
      main_frame_token());
  android_autofill_manager().OnFormsSeen({form1, form2}, /*removed_forms=*/{});

  EXPECT_CALL(
      provider_bridge(),
      StartAutofillSession(EqualsFormData(form1), EqualsFieldInfo(/*index=*/1),
                           /*has_server_predictions=*/false));
  android_autofill_manager().SimulateOnAskForValuesToFill(form1,
                                                          form1.fields[1]);

  EXPECT_CALL(
      provider_bridge(),
      StartAutofillSession(EqualsFormData(form2), EqualsFieldInfo(/*index=*/0),
                           /*has_server_predictions=*/false));
  android_autofill_manager().SimulateOnAskForValuesToFill(form2,
                                                          form2.fields[0]);
}

// Tests that value changes in the form of the Autofill session are propagated
// to Java and to the state that `AutofillProviderAndroid` keeps.
TEST_F(AutofillProviderAndroidTest, OnTextFieldDidChange) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form, form.fields[1]);

  // Simulate a value change.
  EXPECT_CALL(provider_bridge(),
              OnFormFieldDidChange(EqualsFieldInfo(/*index=*/1)));
  form.fields[1].value += u"x";
  android_autofill_manager().SimulateOnTextFieldDidChange(form, form.fields[1]);
  // The `FormDataAndroid` object owned by the provider is also updated.
  ASSERT_TRUE(test_api(autofill_provider()).form());
  EXPECT_EQ(test_api(autofill_provider()).form()->form().fields[1].value,
            form.fields[1].value);
}

// Tests that value changes in a form that is not part of the current Autofill
// session are ignored.
TEST_F(AutofillProviderAndroidTest, OnTextFieldDidChangeInUnrelatedForm) {
  FormData form1 = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  FormData form2 = CreateFormDataForFrame(
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true),
      main_frame_token());
  android_autofill_manager().OnFormsSeen({form1, form2}, /*removed_forms=*/{});

  // Start the Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form1,
                                                          form1.fields[1]);

  // Simulate a value change in a different form.
  EXPECT_CALL(provider_bridge(), OnFormFieldDidChange).Times(0);
  form2.fields[1].value += u"x";
  android_autofill_manager().SimulateOnTextFieldDidChange(form2,
                                                          form2.fields[1]);
}

// Tests that scrolling events in the form of the Autofill session are
// propagated to Java.
TEST_F(AutofillProviderAndroidTest, OnTextFieldDidScroll) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start the Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form, form.fields[2]);

  // Simulate scrolling.
  EXPECT_CALL(provider_bridge(),
              OnTextFieldDidScroll(EqualsFieldInfo(/*index=*/2)));
  android_autofill_manager().SimulateOnTextFieldDidScroll(form, form.fields[2]);
}

// Tests that scrolling envets in a form that is not part of the current
// Autofill session are ignored.
TEST_F(AutofillProviderAndroidTest, OnTextFieldDidScrollInUnrelatedForm) {
  FormData form1 = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  FormData form2 = CreateFormDataForFrame(
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true),
      main_frame_token());
  android_autofill_manager().OnFormsSeen({form1, form2}, /*removed_forms=*/{});

  // Start the Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form1,
                                                          form1.fields[1]);

  // Simulate a scroll event in a different form.
  EXPECT_CALL(provider_bridge(), OnFormFieldDidChange).Times(0);
  android_autofill_manager().SimulateOnTextFieldDidScroll(form2,
                                                          form2.fields[1]);
}

// Tests that a form submission of an ongoing Autofill session is propagated to
// Java if `known_success` is true.
TEST_F(AutofillProviderAndroidTest, OnFormSubmittedWithKnownSuccess) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form, form.fields[0]);

  EXPECT_CALL(provider_bridge(),
              OnFormSubmitted(mojom::SubmissionSource::FORM_SUBMISSION));
  android_autofill_manager().SimulateOnFormSubmitted(
      form, /*known_success=*/true, mojom::SubmissionSource::FORM_SUBMISSION);
}

// Tests that a form submission of an ongoing Autofill session is propagated to
// Java when the `AutofillManager` of the tab is reset, even if the form
// submission was not known to be a success.
TEST_F(AutofillProviderAndroidTest, FormSubmissionHappensOnReset) {
  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager().SimulateOnAskForValuesToFill(form, form.fields[0]);

  EXPECT_CALL(provider_bridge(), OnFormSubmitted).Times(0);
  android_autofill_manager().SimulateOnFormSubmitted(
      form, /*known_success=*/false,
      mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR);
  Mock::VerifyAndClearExpectations(&provider_bridge());

  EXPECT_CALL(provider_bridge(),
              OnFormSubmitted(mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR));
  android_autofill_manager().Reset();
}

// Tests that a form submission of an ongoing Autofill session is propagated to
// Java when the `AutofillManager` of the tab is destroyed. Put differently,
// it tests that the `AutofillManager` is reset on destruction.
TEST_F(AutofillProviderAndroidTest, FormSubmissionHappensOnFrameDestruction) {
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_frame())
          ->AppendChild(std::string("child"));
  child_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://foo.bar"), child_rfh);

  // Force creation of driver.
  ASSERT_TRUE(ContentAutofillDriverFactory::FromWebContents(web_contents())
                  ->DriverForFrame(child_rfh));

  FormData form = CreateFormDataForFrame(
      CreateTestPersonalInformationFormData(),
      LocalFrameToken(child_rfh->GetFrameToken().value()));
  android_autofill_manager(child_rfh).OnFormsSeen({form},
                                                  /*removed_forms=*/{});

  // Start an Autofill session.
  android_autofill_manager(child_rfh).SimulateOnAskForValuesToFill(
      form, form.fields[0]);

  EXPECT_CALL(provider_bridge(), OnFormSubmitted).Times(0);
  android_autofill_manager(child_rfh).SimulateOnFormSubmitted(
      form, /*known_success=*/false,
      mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR);
  Mock::VerifyAndClearExpectations(&provider_bridge());

  EXPECT_CALL(provider_bridge(),
              OnFormSubmitted(mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR));
  content::RenderFrameHostTester::For(std::exchange(child_rfh, nullptr))
      ->Detach();
}

// Tests that no prefill requests are sent on Android versions prior to U even
// if all other requirements are satisfied.
TEST_F(AutofillProviderAndroidTest, NoPrefillRequestOnVersionsPriorToU) {
  // This test only makes sense on Android versions smaller than U.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

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
TEST_F(AutofillProviderAndroidTest, SendPrefillRequest) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  // Upon receiving server predictions a prefill request should be sent.
  EXPECT_CALL(provider_bridge(), SendPrefillRequest(EqualsFormData(form)));
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
}

// Tests that no prefill request is sent if the feature is disabled.
TEST_F(AutofillProviderAndroidTest, NoPrefillRequestWithoutFeature) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

  FormData form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  ASSERT_TRUE(android_autofill_manager().FindCachedFormById(form.global_id()));

  // Upon receiving server predictions a prefill request should be sent.
  EXPECT_CALL(provider_bridge(), SendPrefillRequest).Times(0);
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      form.global_id());
}

// Tests that no prefill request is sent if there is already an ongoing Autofill
// session.
TEST_F(AutofillProviderAndroidTest, NoPrefillRequestIfOngoingSession) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

  FormData login_form1 =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form1}, /*removed_forms=*/{});
  EXPECT_CALL(provider_bridge(), StartAutofillSession);
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form1, login_form1.fields.front());
  histogram_tester.ExpectUniqueSample(
      AutofillProviderAndroid::kPrefillRequestStateUma,
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
TEST_F(AutofillProviderAndroidTest, NoSecondPrefillRequest) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

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
      login_form2, login_form2.fields.front());
  histogram_tester.ExpectUniqueSample(
      AutofillProviderAndroid::kPrefillRequestStateUma,
      PrefillRequestState::kRequestNotSentMaxNumberReached, 1);
}

// Tests that the session id used in a prefill request is also used for starting
// the Autofill session for that form.
TEST_F(AutofillProviderAndroidTest, SessionIdIsReusedForCachedForms) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

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
  android_autofill_manager().SimulateOnAskForValuesToFill(form,
                                                          form.fields.front());
}

// Tests that the session id used in a prefill request is not reused when
// starting a session on a form with the same id, but changed field content.
TEST_F(AutofillProviderAndroidTest,
       SessionIdIsNotReusedForCachedFormsIfContentHasChanged) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

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
  changed_form.fields.pop_back();
  android_autofill_manager().OnFormsSeen({changed_form},
                                         /*removed_forms=*/{form.global_id()});
  SessionId autofill_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(),
              StartAutofillSession(EqualsFormData(changed_form),
                                   EqualsFieldInfo(/*index=*/0),
                                   /*has_server_predictions=*/true))
      .WillOnce(WithArg<0>(SaveSessionId(&autofill_session_id)));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      changed_form, changed_form.fields.front());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  // A new session id is used to start the Autofill session.
  EXPECT_NE(cache_session_id, autofill_session_id);
  histogram_tester.ExpectUniqueSample(
      AutofillProviderAndroid::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentFormChanged, 1);
}

// Tests that the session id used in a prefill request is only used once to
// start an Autofill session. If the user then focuses on a different form
// before returning to the (formerly) cached form, a new session is started.
TEST_F(AutofillProviderAndroidTest,
       SessionIdIsNotReusedMultipleAutofillSessions) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

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
      pw_form, pw_form.fields.front());
  Mock::VerifyAndClearExpectations(&provider_bridge());

  // Now focus on a different form.
  SessionId pi_form_session_id = SessionId(0);
  EXPECT_CALL(provider_bridge(),
              StartAutofillSession(EqualsFormData(pi_form),
                                   EqualsFieldInfo(/*index=*/0),
                                   /*has_server_predictions=*/false))
      .WillOnce(WithArg<0>(SaveSessionId(&pi_form_session_id)));
  android_autofill_manager().SimulateOnAskForValuesToFill(
      pi_form, pi_form.fields.front());
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
      pw_form, pw_form.fields.front());
  Mock::VerifyAndClearExpectations(&provider_bridge());
  // The session id used when focusing back should be different from both those
  // before.
  EXPECT_NE(cache_session_id, pw_form_second_session_id);
  EXPECT_NE(pi_form_session_id, pw_form_second_session_id);
}

// Tests that metrics are emitted when the bottom sheet is shown.
TEST_F(AutofillProviderAndroidTest,
       PrefillRequestStateEmittedOnShowingBottomSheet) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

  FormData login_form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form.global_id());

  EXPECT_CALL(provider_bridge(), StartAutofillSession);
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form, login_form.fields.front());

  // Simulate a successfully shown bottom sheet.
  provider_bridge_delegate().OnShowBottomSheetResult(
      /*is_shown=*/true, /*provided_autofill_structure=*/true);
  histogram_tester.ExpectUniqueSample(
      AutofillProviderAndroid::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentStructureProvidedBottomSheetShown, 1);
}

// Tests that the correct metrics are emitted when the bottom sheet is not shown
// and no view structure was provided to the Android framework.
TEST_F(AutofillProviderAndroidTest,
       PrefillRequestStateEmittedOnNotShowingBottomSheetWithoutViewStructure) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

  FormData login_form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form.global_id());
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form, login_form.fields.front());

  // Simulate a successfully shown bottom sheet.
  provider_bridge_delegate().OnShowBottomSheetResult(
      /*is_shown=*/false, /*provided_autofill_structure=*/false);
  histogram_tester.ExpectUniqueSample(
      AutofillProviderAndroid::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentStructureNotProvided, 1);
}

// Tests that the correct metrics are emitted when the bottom sheet is not shown
// and a view structure was provided to the Android framework.
TEST_F(AutofillProviderAndroidTest,
       PrefillRequestStateEmittedOnNotShowingBottomSheetWithViewStructure) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_U) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidAutofillPrefillRequestsForLoginForms);

  FormData login_form =
      CreateFormDataForFrame(CreateTestLoginForm(), main_frame_token());
  android_autofill_manager().OnFormsSeen({login_form}, /*removed_forms=*/{});
  android_autofill_manager().SimulatePropagateAutofillPredictions(
      login_form.global_id());
  android_autofill_manager().SimulateOnAskForValuesToFill(
      login_form, login_form.fields.front());

  // Simulate a successfully shown bottom sheet.
  provider_bridge_delegate().OnShowBottomSheetResult(
      /*is_shown=*/false, /*provided_autofill_structure=*/true);
  histogram_tester.ExpectUniqueSample(
      AutofillProviderAndroid::kPrefillRequestStateUma,
      PrefillRequestState::kRequestSentStructureProvidedBottomSheetNotShown, 1);
}

class AutofillProviderAndroidTestHidingLogic
    : public AutofillProviderAndroidTest {
 public:
  void SetUp() override {
    AutofillProviderAndroidTest::SetUp();
    NavigateAndCommit(GURL("https://foo.com"));
    sub_frame_ = content::RenderFrameHostTester::For(main_frame())
                     ->AppendChild(std::string("child"));
    sub_frame_ = NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com"));
    // Make sure the driver (and the manager) is created as there is an early
    // return in `ContentAutofillDriverFactory::DidFinishNavigation` before
    // `DriverForFrame()` call.
    ContentAutofillDriverFactory::FromWebContents(web_contents())
        ->DriverForFrame(sub_frame_);
  }

  void TearDown() override {
    sub_frame_ = nullptr;
    AutofillProviderAndroidTest::TearDown();
  }

  void AskForValuesToFill(content::RenderFrameHost* rfh) {
    FocusWebContentsOnFrame(rfh);
    FormData form =
        CreateFormDataForFrame(CreateTestPersonalInformationFormData(),
                               LocalFrameToken(rfh->GetFrameToken().value()));
    android_autofill_manager(rfh).OnFormsSeen({form},
                                              /*removed_forms=*/{});
    // Start an Autofill session.
    android_autofill_manager(rfh).SimulateOnAskForValuesToFill(form,
                                                               form.fields[0]);
  }

 protected:
  raw_ptr<content::RenderFrameHost> sub_frame_ = nullptr;
};

// Tests that if the popup is shown in the *main frame*, destruction of the
// *sub frame* does not hide the popup.
TEST_F(AutofillProviderAndroidTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameDestruction) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(0);
  content::RenderFrameHostTester::For(sub_frame_)->Detach();
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&provider_bridge());
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *sub frame* does not hide the popup.
TEST_F(AutofillProviderAndroidTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameNavigation) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(0);
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&provider_bridge());
}

// Tests that if the popup is shown in the *main frame*, destruction of the
// *main frame* resets the java instance which hides the popup.
TEST_F(AutofillProviderAndroidTestHidingLogic, HideInMainFrameOnDestruction) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), Reset);
  // TearDown() destructs the main frame.
}

// Tests that if the popup is shown in the *sub frame*, destruction of the
// *sub frame* hides the popup.
TEST_F(AutofillProviderAndroidTestHidingLogic, HideInSubFrameOnDestruction) {
  AskForValuesToFill(sub_frame_);
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&provider_bridge());
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AutofillProviderAndroidTestHidingLogic,
       HideInMainFrameOnMainFrameNavigation) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *sub frame* hides the popup.
//
// TODO(crbug.com/1488233): Disabled because AutofillProviderAndroid::Reset()
// resets AutofillProviderAndroid::field_rfh_ before RenderFrameDeleted(), which
// prevents OnPopupHidden().
TEST_F(AutofillProviderAndroidTestHidingLogic,
       DISABLED_HideInSubFrameOnSubFrameNavigation) {
  AskForValuesToFill(sub_frame_);
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AutofillProviderAndroidTestHidingLogic,
       HideInSubFrameOnMainFrameNavigation) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

// Tests that AutofillProviderAndroid::last_queried_field_rfh_id_ is updated
// when different frames are queried.
TEST_F(AutofillProviderAndroidTestHidingLogic,
       FollowAskForValuesInDifferentFrames) {
  AskForValuesToFill(main_frame());
  AskForValuesToFill(sub_frame_);
  EXPECT_CALL(provider_bridge(), HideDatalistPopup).Times(AtLeast(1));
  NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com/"));
}

}  // namespace autofill
