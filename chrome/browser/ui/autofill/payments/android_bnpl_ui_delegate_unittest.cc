// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/android_bnpl_ui_delegate.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class MockPaymentsAutofillClient : public payments::TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : payments::TestPaymentsAutofillClient(client) {}
  ~MockPaymentsAutofillClient() override = default;

  MOCK_METHOD(bool,
              ShowTouchToFillProgress,
              (base::OnceClosure cancel_callback),
              (override));
};

class AndroidBnplUiDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  AndroidBnplUiDelegateTest() = default;
  ~AndroidBnplUiDelegateTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    autofill_client()->set_payments_autofill_client(
        std::make_unique<testing::NiceMock<MockPaymentsAutofillClient>>(
            autofill_client()));
    delegate_ =
        std::make_unique<AndroidBnplUiDelegate>(&payments_autofill_client());
  }

 protected:
  TestContentAutofillClient* autofill_client() {
    return client_injector_[web_contents()];
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return static_cast<MockPaymentsAutofillClient&>(
        *autofill_client()->GetPaymentsAutofillClient());
  }

  std::unique_ptr<AndroidBnplUiDelegate> delegate_;

 private:
  TestAutofillClientInjector<TestContentAutofillClient> client_injector_;
};

// Tests that ShowProgressUi calls the client's ShowTouchToFillProgress.
TEST_F(AndroidBnplUiDelegateTest, ShowProgressUi) {
  EXPECT_CALL(payments_autofill_client(),
              ShowTouchToFillProgress(testing::An<base::OnceClosure>()));

  delegate_->ShowProgressUi(
      AutofillProgressDialogType::kBnplFetchVcnProgressDialog,
      /*cancel_callback=*/base::DoNothing());
}

}  // namespace autofill::payments
