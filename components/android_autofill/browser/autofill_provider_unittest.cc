// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/autofill_provider_android.h"
#include "components/android_autofill/browser/autofill_provider_android_bridge.h"
#include "components/android_autofill/browser/mock_form_field_data_android_bridge.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class TestAndroidAutofillManager : public AndroidAutofillManager {
 public:
  explicit TestAndroidAutofillManager(ContentAutofillDriver* driver,
                                      ContentAutofillClient* client)
      : AndroidAutofillManager(driver, client) {}

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
              (base::span<const std::u16string>,
               base::span<const std::u16string>,
               bool),
              (override));
  MOCK_METHOD(void, HideDatalistPopup, (), (override));
  MOCK_METHOD(void,
              OnFocusChanged,
              (const absl::optional<FieldInfo>&),
              (override));
  MOCK_METHOD(void, OnFormFieldDidChange, (const FieldInfo&), (override));
  MOCK_METHOD(void, OnTextFieldDidScroll, (const FieldInfo&), (override));
  MOCK_METHOD(void, OnFormSubmitted, (mojom::SubmissionSource), (override));
  MOCK_METHOD(void, OnDidFillAutofillFormData, (), (override));
};

}  // namespace

class AutofillProviderTest : public content::RenderViewHostTestHarness {
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
  }

  void TearDown() override {
    provider_bridge_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  TestAndroidAutofillManager& android_autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  AutofillProvider& autofill_provider() {
    return *AutofillProvider::FromWebContents(web_contents());
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
TEST_F(AutofillProviderTest, HasServerPrediction) {
  FormData form = test::CreateTestPersonalInformationFormData();
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

}  // namespace autofill
