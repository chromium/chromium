// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_handler_proxy.h"
#include "components/autofill/core/browser/test_autofill_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillHandlerProxyTestHelper : public AutofillHandlerProxy {
 public:
  explicit AutofillHandlerProxyTestHelper(AutofillProvider* autofill_provider)
      : AutofillHandlerProxy(nullptr,
                             nullptr,
                             autofill_provider,
                             DISABLE_AUTOFILL_DOWNLOAD_MANAGER) {}

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
  bool HasServerPrediction() const { return handler_->has_server_prediction(); }

 private:
  // AutofillProvider
  void OnQueryFormFieldAutofill(AutofillHandlerProxy* handler,
                                int32_t id,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                bool autoselect_first_suggestion) override {
    handler_ = handler;
  }
  void OnServerQueryRequestError(AutofillHandlerProxy* handler,
                                 FormSignature form_signature) override {}

  AutofillHandlerProxy* handler_;
};

class AutofillProviderTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_provider_test_helper_ =
        std::make_unique<AutofillProviderTestHelper>();
    autofill_handler_proxy_test_helper_ =
        std::make_unique<AutofillHandlerProxyTestHelper>(
            autofill_provider_test_helper_.get());
  }

  AutofillProviderTestHelper* autofill_provider_test_helper() {
    return autofill_provider_test_helper_.get();
  }

  AutofillHandlerProxyTestHelper* autofill_handler_proxy_test_helper() {
    return autofill_handler_proxy_test_helper_.get();
  }

 private:
  std::unique_ptr<AutofillProviderTestHelper> autofill_provider_test_helper_;
  std::unique_ptr<AutofillHandlerProxyTestHelper>
      autofill_handler_proxy_test_helper_;
};

TEST_F(AutofillProviderTest, HasServerPredictionAfterQuery) {
  // Simulate the result arrives after starting autofill.
  autofill_handler_proxy_test_helper()->SimulateOnQueryFormFieldAutofillImpl();
  EXPECT_FALSE(autofill_provider_test_helper()->HasServerPrediction());
  autofill_handler_proxy_test_helper()->SimulatePropagateAutofillPredictions();
  EXPECT_TRUE(autofill_provider_test_helper()->HasServerPrediction());
  autofill_handler_proxy_test_helper()->Reset();
  EXPECT_FALSE(autofill_provider_test_helper()->HasServerPrediction());
}

TEST_F(AutofillProviderTest, HasServerPredictionBeforeQuery) {
  // Simulate the result arrives before starting autofill.
  autofill_handler_proxy_test_helper()->SimulatePropagateAutofillPredictions();
  autofill_handler_proxy_test_helper()->SimulateOnQueryFormFieldAutofillImpl();
  EXPECT_TRUE(autofill_provider_test_helper()->HasServerPrediction());
  autofill_handler_proxy_test_helper()->Reset();
  EXPECT_FALSE(autofill_provider_test_helper()->HasServerPrediction());
}

}  // namespace autofill
