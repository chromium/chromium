// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/test_autofill_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AndroidAutofillManagerTestHelper : public AndroidAutofillManager {
 public:
  explicit AndroidAutofillManagerTestHelper(AutofillProvider* autofill_provider)
      : AndroidAutofillManager(nullptr, nullptr) {
    set_autofill_provider_for_testing(autofill_provider);
  }

  void SimulatePropagateAutofillPredictions() {
    PropagateAutofillPredictions({});
  }

  void SimulateOnAskForValuesToFillImpl() {
    OnAskForValuesToFillImpl(FormData(), FormFieldData(), gfx::RectF(),
                             AutoselectFirstSuggestion(false),
                             FormElementWasClicked(false));
  }
};

class AutofillProviderTestHelper : public TestAutofillProvider {
 public:
  explicit AutofillProviderTestHelper(content::WebContents* web_contents)
      : TestAutofillProvider(web_contents) {}

  bool HasServerPrediction() const { return manager_->has_server_prediction(); }

 private:
  // AutofillProvider
  void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutoselectFirstSuggestion autoselect_first_suggestion,
      FormElementWasClicked form_element_was_clicked) override {
    manager_ = manager;
  }
  void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                 FormSignature form_signature) override {}

  raw_ptr<AndroidAutofillManager> manager_;
};

class AutofillProviderTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    // Owned by WebContents.
    autofill_provider_test_helper_ =
        new AutofillProviderTestHelper(web_contents_.get());
    android_autofill_manager_test_helper_ =
        std::make_unique<AndroidAutofillManagerTestHelper>(
            autofill_provider_test_helper_.get());
  }

  AutofillProviderTestHelper* autofill_provider_test_helper() {
    return autofill_provider_test_helper_.get();
  }

  AndroidAutofillManagerTestHelper* android_autofill_manager_test_helper() {
    return android_autofill_manager_test_helper_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  // Owned by WebContents.
  raw_ptr<AutofillProviderTestHelper> autofill_provider_test_helper_;
  std::unique_ptr<AndroidAutofillManagerTestHelper>
      android_autofill_manager_test_helper_;
};

TEST_F(AutofillProviderTest, HasServerPredictionAfterQuery) {
  // Simulate the result arrives after starting autofill.
  android_autofill_manager_test_helper()->SimulateOnAskForValuesToFillImpl();
  EXPECT_FALSE(autofill_provider_test_helper()->HasServerPrediction());
  android_autofill_manager_test_helper()
      ->SimulatePropagateAutofillPredictions();
  EXPECT_TRUE(autofill_provider_test_helper()->HasServerPrediction());
  android_autofill_manager_test_helper()->Reset();
  EXPECT_FALSE(autofill_provider_test_helper()->HasServerPrediction());
}

TEST_F(AutofillProviderTest, HasServerPredictionBeforeQuery) {
  // Simulate the result arrives before starting autofill.
  android_autofill_manager_test_helper()
      ->SimulatePropagateAutofillPredictions();
  android_autofill_manager_test_helper()->SimulateOnAskForValuesToFillImpl();
  EXPECT_TRUE(autofill_provider_test_helper()->HasServerPrediction());
  android_autofill_manager_test_helper()->Reset();
  EXPECT_FALSE(autofill_provider_test_helper()->HasServerPrediction());
}

}  // namespace autofill
