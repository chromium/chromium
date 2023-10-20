// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider.h"

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
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
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::ResultOf;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;
using FieldInfo = AutofillProviderAndroidBridge::FieldInfo;
using test::CreateFormDataForFrame;
using test::CreateTestCreditCardFormData;
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
              return std::make_unique<MockFormFieldDataAndroidBridge>();
            }));
    AndroidAutofillBridgeFactory::GetInstance()
        .SetAutofillProviderAndroidTestingFactory(base::BindLambdaForTesting(
            [&bridge_ptr = provider_bridge_](
                AutofillProviderAndroidBridge::Delegate* delegate)
                -> std::unique_ptr<AutofillProviderAndroidBridge> {
              auto bridge =
                  std::make_unique<MockAutofillProviderAndroidBridge>();
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

class AutofillProviderAndroidTestHidingLogic
    : public AutofillProviderAndroidTest {
 public:
  void SetUp() override {
    AutofillProviderAndroidTest::SetUp();
    NavigateAndCommit(GURL("https://foo.com"));
    sub_frame_ = content::RenderFrameHostTester::For(main_frame())
                     ->AppendChild(std::string("child"));
    sub_frame_ = NavigateAndCommitFrame(sub_frame_, GURL("https://bar.com"));
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
// *main frame* hides the popup.
TEST_F(AutofillProviderAndroidTestHidingLogic, HideInMainFrameOnDestruction) {
  AskForValuesToFill(main_frame());
  EXPECT_CALL(provider_bridge(), HideDatalistPopup);
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
