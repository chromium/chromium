// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/payments/payments_window_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/ui/android/autofill/payments/payments_window_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

namespace autofill::payments {

class MockPaymentsWindowDelegate : public PaymentsWindowDelegate {
 public:
  MockPaymentsWindowDelegate() = default;

  MOCK_METHOD(void, OnDidFinishNavigationForBnpl, (const GURL&), (override));
  MOCK_METHOD(void,
              OnWebContentsObservationStarted,
              (content::WebContents&),
              (override));
  MOCK_METHOD(void, WebContentsDestroyed, (), (override));
};

class PaymentsWindowBridgeTest : public ChromeRenderViewHostTestHarness {
 public:
  PaymentsWindowBridgeTest() : env_(base::android::AttachCurrentThread()) {}

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    payments_window_bridge_ =
        std::make_unique<PaymentsWindowBridge>(&mock_delegate_);
  }

  TestContentAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

  testing::StrictMock<MockPaymentsWindowDelegate> mock_delegate_;
  std::unique_ptr<PaymentsWindowBridge> payments_window_bridge_;
  raw_ptr<JNIEnv> env_;

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

TEST_F(PaymentsWindowBridgeTest, OnNavigationFinished_ForwardsCallToDelegate) {
  GURL clicked_url("https://www.bnpltest.com/");
  EXPECT_CALL(mock_delegate_, OnDidFinishNavigationForBnpl(clicked_url));

  payments_window_bridge_->OnNavigationFinished(
      env_, url::GURLAndroid::FromNativeGURL(env_, clicked_url));
}

TEST_F(PaymentsWindowBridgeTest,
       OnWebContentsObservationStarted_ForwardsCallToDelegate) {
  EXPECT_CALL(mock_delegate_,
              OnWebContentsObservationStarted(testing::Ref(*web_contents())));

  payments_window_bridge_->OnWebContentsObservationStarted(
      env_, web_contents()->GetJavaWebContents());
}

TEST_F(PaymentsWindowBridgeTest,
       OnWebContentsDestroyed_ForwardsCallToDelegate) {
  EXPECT_CALL(mock_delegate_, WebContentsDestroyed());

  payments_window_bridge_->OnWebContentsDestroyed(env_);
}
}  // namespace autofill::payments
