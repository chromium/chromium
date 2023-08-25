// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider.h"

#include "base/memory/raw_ptr.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/test_autofill_provider.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestAndroidAutofillManager : public AndroidAutofillManager {
 public:
  explicit TestAndroidAutofillManager(ContentAutofillDriver* driver,
                                      ContentAutofillClient* client)
      : AndroidAutofillManager(driver, client) {}

  void SimulatePropagateAutofillPredictions(FormGlobalId form_id) {
    NotifyObservers(&Observer::OnFieldTypesDetermined, form_id,
                    Observer::FieldTypeSource::kAutofillServer);
  }

  void SimulateOnAskForValuesToFillImpl(FormGlobalId form_id) {
    FormData form;
    form.host_frame = form_id.frame_token;
    form.unique_renderer_id = form_id.renderer_id;

    FormFieldData field;
    field.host_frame = form.host_frame;
    field.unique_renderer_id = test::MakeFieldRendererId();
    OnAskForValuesToFillImpl(
        form, field, gfx::RectF(),
        AutofillSuggestionTriggerSource::kTextFieldDidChange);
  }
};

class FakeAutofillProvider : public TestAutofillProvider {
 public:
  using TestAutofillProvider::TestAutofillProvider;

  bool HasServerPrediction(FormGlobalId form_id) const {
    return manager_->has_server_prediction(form_id);
  }

 private:
  // AutofillProvider:
  void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source) override {
    manager_ = manager;
  }

  void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                 FormSignature form_signature) override {}

  raw_ptr<AndroidAutofillManager> manager_ = nullptr;
};

class AutofillProviderTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    CreateAutofillProvider();
    NavigateAndCommit(GURL("about:blank"));
  }

  TestAndroidAutofillManager& android_autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  FakeAutofillProvider& autofill_provider() {
    return *static_cast<FakeAutofillProvider*>(
        FakeAutofillProvider::FromWebContents(web_contents()));
  }

 private:
  // The AutofillProvider is owned by the `web_contents()`. It registers itself
  // as WebContentsUserData.
  void CreateAutofillProvider() {
    ASSERT_FALSE(FakeAutofillProvider::FromWebContents(web_contents()));
    new FakeAutofillProvider(web_contents());
    ASSERT_TRUE(FakeAutofillProvider::FromWebContents(web_contents()));
  }

  test::AutofillUnitTestEnvironment autofill_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestAndroidAutofillManager>
      autofill_manager_injector_;
};

TEST_F(AutofillProviderTest, HasServerPredictionAfterQuery) {
  // Simulate the result arrives after starting autofill.
  FormGlobalId form_id = test::MakeFormGlobalId();
  android_autofill_manager().SimulateOnAskForValuesToFillImpl(form_id);
  EXPECT_FALSE(autofill_provider().HasServerPrediction(form_id));
  android_autofill_manager().SimulatePropagateAutofillPredictions(form_id);
  EXPECT_TRUE(autofill_provider().HasServerPrediction(form_id));
  android_autofill_manager().Reset();
  EXPECT_FALSE(autofill_provider().HasServerPrediction(form_id));
}

TEST_F(AutofillProviderTest, HasServerPredictionBeforeQuery) {
  // Simulate the result arrives before starting autofill.
  FormGlobalId form_id = test::MakeFormGlobalId();
  android_autofill_manager().SimulatePropagateAutofillPredictions(form_id);
  android_autofill_manager().SimulateOnAskForValuesToFillImpl(form_id);
  EXPECT_TRUE(autofill_provider().HasServerPrediction(form_id));
  android_autofill_manager().Reset();
  EXPECT_FALSE(autofill_provider().HasServerPrediction(form_id));
}

}  // namespace autofill
