// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestMandatoryReauthBubbleControllerImpl
    : public MandatoryReauthBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestMandatoryReauthBubbleControllerImpl>(
            web_contents));
  }

  explicit TestMandatoryReauthBubbleControllerImpl(
      content::WebContents* web_contents)
      : MandatoryReauthBubbleControllerImpl(web_contents) {}
};

class MandatoryReauthBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  MandatoryReauthBubbleControllerImplTest() = default;
  MandatoryReauthBubbleControllerImplTest(
      MandatoryReauthBubbleControllerImplTest&) = delete;
  MandatoryReauthBubbleControllerImplTest& operator=(
      MandatoryReauthBubbleControllerImplTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestMandatoryReauthBubbleControllerImpl::CreateForTesting(web_contents);
  }

  void ShowBubble() {
    controller()->ShowBubble(accept_callback.Get(), cancel_callback.Get(),
                             close_callback.Get());
  }

  void ClickAcceptButton() {
    controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kAccepted);
  }

  void ClickCancelButton() {
    controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kCancelled);
  }

  void CloseBubble() {
    controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kClosed);
  }

  base::MockOnceClosure accept_callback;
  base::MockOnceClosure cancel_callback;
  base::MockRepeatingClosure close_callback;

 protected:
  TestMandatoryReauthBubbleControllerImpl* controller() {
    return static_cast<TestMandatoryReauthBubbleControllerImpl*>(
        TestMandatoryReauthBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  base::WeakPtrFactory<MandatoryReauthBubbleControllerImplTest>
      weak_ptr_factory_{this};
};

TEST_F(MandatoryReauthBubbleControllerImplTest,
       SuccessfullyInvokesAcceptCallback) {
  ShowBubble();
  EXPECT_CALL(accept_callback, Run).Times(1);
  ClickAcceptButton();
}

TEST_F(MandatoryReauthBubbleControllerImplTest,
       SuccessfullyInvokesCancelCallback) {
  ShowBubble();
  EXPECT_CALL(cancel_callback, Run).Times(1);
  ClickCancelButton();
}

TEST_F(MandatoryReauthBubbleControllerImplTest,
       SuccessfullyInvokesCloseCallback) {
  ShowBubble();
  EXPECT_CALL(close_callback, Run).Times(1);
  CloseBubble();
}

}  // namespace autofill
