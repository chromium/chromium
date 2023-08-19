// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/test_autofill_provider.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestAndroidAutofillManager : public AndroidAutofillManager {
 public:
  explicit TestAndroidAutofillManager(ContentAutofillDriver* driver,
                                      ContentAutofillClient* client)
      : AndroidAutofillManager(driver, client) {}

  void SimulatePropagateAutofillPredictions() {
    PropagateAutofillPredictionsDeprecated({});
  }

  void SimulateOnAskForValuesToFillImpl() {
    OnAskForValuesToFillImpl(
        FormData(), FormFieldData(), gfx::RectF(),
        AutofillSuggestionTriggerSource::kTextFieldDidChange);
  }
};

class FakeAutofillProvider : public TestAutofillProvider {
 public:
  using TestAutofillProvider::TestAutofillProvider;

  bool HasServerPrediction() const { return manager_->has_server_prediction(); }

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

  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestAndroidAutofillManager>
      autofill_manager_injector_;
};

TEST_F(AutofillProviderTest, HasServerPredictionAfterQuery) {
  // Simulate the result arrives after starting autofill.
  android_autofill_manager().SimulateOnAskForValuesToFillImpl();
  EXPECT_FALSE(autofill_provider().HasServerPrediction());
  android_autofill_manager().SimulatePropagateAutofillPredictions();
  EXPECT_TRUE(autofill_provider().HasServerPrediction());
  android_autofill_manager().Reset();
  EXPECT_FALSE(autofill_provider().HasServerPrediction());
}

TEST_F(AutofillProviderTest, HasServerPredictionBeforeQuery) {
  // Simulate the result arrives before starting autofill.
  android_autofill_manager().SimulatePropagateAutofillPredictions();
  android_autofill_manager().SimulateOnAskForValuesToFillImpl();
  EXPECT_TRUE(autofill_provider().HasServerPrediction());
  android_autofill_manager().Reset();
  EXPECT_FALSE(autofill_provider().HasServerPrediction());
}

}  // namespace autofill
