// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/payments/autofill_payments_window_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/ui/android/autofill/payments/autofill_payments_window_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

namespace autofill::payments {

class MockAutofillPaymentsWindowDelegate
    : public AutofillPaymentsWindowDelegate {
 public:
  MockAutofillPaymentsWindowDelegate() = default;

  MOCK_METHOD(void, OnDidFinishNavigationForBnpl, (const GURL&), (override));
  MOCK_METHOD(void, WebContentsDestroyed, (), (override));
};

class AutofillPaymentsWindowBridgeTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillPaymentsWindowBridgeTest()
      : env_(base::android::AttachCurrentThread()) {}

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    autofill_payments_window_bridge_ =
        std::make_unique<AutofillPaymentsWindowBridge>(*web_contents(),
                                                       &mock_delegate_);
  }

  TestContentAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

  testing::StrictMock<MockAutofillPaymentsWindowDelegate> mock_delegate_;
  std::unique_ptr<AutofillPaymentsWindowBridge>
      autofill_payments_window_bridge_;
  raw_ptr<JNIEnv> env_;

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

TEST_F(AutofillPaymentsWindowBridgeTest,
       OnNavigationFinished_ForwardsCallToDelegate) {
  GURL clicked_url("https://www.bnpltest.com/");
  EXPECT_CALL(mock_delegate_, OnDidFinishNavigationForBnpl(clicked_url));

  autofill_payments_window_bridge_->OnNavigationFinished(
      env_, url::GURLAndroid::FromNativeGURL(env_, clicked_url));
}

TEST_F(AutofillPaymentsWindowBridgeTest,
       OnWebContentsDestroyed_ForwardsCallToDelegate) {
  EXPECT_CALL(mock_delegate_, WebContentsDestroyed());

  autofill_payments_window_bridge_->OnWebContentsDestroyed(env_);
}
}  // namespace autofill::payments
