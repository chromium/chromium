// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/test_autofill_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AndroidAutofillManagerTestHelper : public AndroidAutofillManager {
 public:
  explicit AndroidAutofillManagerTestHelper(AutofillProvider* autofill_provider)
      : AndroidAutofillManager(nullptr,
                               nullptr,
                               DISABLE_AUTOFILL_DOWNLOAD_MANAGER) {
    set_autofill_provider_for_testing(autofill_provider);
  }

  void SimulatePropagateAutofillPredictions() {
    PropagateAutofillPredictions(nullptr, std::vector<FormStructure*>());
  }

  void SimulateOnQueryFormFieldAutofillImpl() {
    OnQueryFormFieldAutofillImpl(0, FormData(), FormFieldData(), gfx::RectF(),
                                 /*autoselect_first_suggestion=*/false);
  }
};

class AutofillProviderTestHelper : public TestAutofillProvider {
 public:
  bool HasServerPrediction() const { return manager_->has_server_prediction(); }

 private:
  // AutofillProvider
  void OnQueryFormFieldAutofill(AndroidAutofillManager* manager,
                                int32_t id,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                bool autoselect_first_suggestion) override {
    manager_ = manager;
  }
  void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                 FormSignature form_signature) override {}

  AndroidAutofillManager* manager_;
};

class AutofillProviderTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_provider_test_helper_ =
        std::make_unique<AutofillProviderTestHelper>();
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
  std::unique_ptr<AutofillProviderTestHelper> autofill_provider_test_helper_;
  std::unique_ptr<AndroidAutofillManagerTestHelper>
      android_autofill_manager_test_helper_;
};

TEST_F(AutofillProviderTest, HasServerPredictionAfterQuery) {
  // Simulate the result arrives after starting autofill.
  android_autofill_manager_test_helper()
      ->SimulateOnQueryFormFieldAutofillImpl();
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
  android_autofill_manager_test_helper()
      ->SimulateOnQueryFormFieldAutofillImpl();
  EXPECT_TRUE(autofill_provider_test_helper()->HasServerPrediction());
  android_autofill_manager_test_helper()->Reset();
  EXPECT_FALSE(autofill_provider_test_helper()->HasServerPrediction());
}

}  // namespace autofill
