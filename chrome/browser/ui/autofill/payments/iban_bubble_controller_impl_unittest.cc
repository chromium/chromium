// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestIbanBubbleControllerImpl : public IbanBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestIbanBubbleControllerImpl>(web_contents));
  }

  explicit TestIbanBubbleControllerImpl(content::WebContents* web_contents)
      : IbanBubbleControllerImpl(web_contents) {}
};

class IbanBubbleControllerImplTest : public BrowserWithTestWindowTest {
 public:
  explicit IbanBubbleControllerImplTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : BrowserWithTestWindowTest(time_source) {}
  IbanBubbleControllerImplTest(IbanBubbleControllerImplTest&) = delete;
  IbanBubbleControllerImplTest& operator=(IbanBubbleControllerImplTest&) =
      delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestIbanBubbleControllerImpl::CreateForTesting(web_contents);
  }

  void ShowLocalSaveBubble(const Iban& iban) {
    controller()->OfferLocalSave(
        iban, /*should_show_prompt=*/true,
        base::BindOnce(&IbanBubbleControllerImplTest::LocalSaveIbanCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void ClickSaveButton(const std::u16string& nickname) {
    controller()->OnAcceptButton(nickname);
    controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kAccepted);
    if (controller()->ShouldShowPaymentSavedLabelAnimation()) {
      controller()->OnAnimationEnded();
    }
  }

  void ShowConfirmationBubbleView(bool iban_saved, bool hit_max_strikes) {
    controller()->ShowConfirmationBubbleView(iban_saved, hit_max_strikes);
  }

  void CloseBubble(PaymentsBubbleClosedReason closed_reason) {
    controller()->OnBubbleClosed(closed_reason);
  }

  std::u16string_view saved_nickname() { return saved_nickname_; }

 protected:
  TestIbanBubbleControllerImpl* controller() {
    return static_cast<TestIbanBubbleControllerImpl*>(
        TestIbanBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  void LocalSaveIbanCallback(
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname) {
    saved_nickname_ = nickname;
  }

  std::u16string_view saved_nickname_;
  base::WeakPtrFactory<IbanBubbleControllerImplTest> weak_ptr_factory_{this};
};

TEST_F(IbanBubbleControllerImplTest, LocalIbanSavedSuccessfully) {
  std::u16string nickname = u"My doctor's IBAN";
  ShowLocalSaveBubble(autofill::test::GetLocalIban());
  ClickSaveButton(nickname);

  EXPECT_EQ(nickname, saved_nickname());
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanOffered) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(autofill::test::GetLocalIban());

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_Accepted) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(autofill::test::GetLocalIban());
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kAccepted, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_Cancelled) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(autofill::test::GetLocalIban());
  CloseBubble(PaymentsBubbleClosedReason::kCancelled);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kCancelled, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_NotInteracted) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(autofill::test::GetLocalIban());
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kNotInteracted, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_LostFocus) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(autofill::test::GetLocalIban());
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kLostFocus, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanSaved_WithNickname) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(autofill::test::GetLocalIban());
  ClickSaveButton(u"My doctor's IBAN");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", true, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanSaved_NoNickname) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(autofill::test::GetLocalIban());
  ClickSaveButton(u"");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", false, 1);
}

// Test that confirmation prompt is auto-closed in 3 sec if the IBAN was
// successfully saved to the server.
TEST_F(IbanBubbleControllerImplTest, OnConfirmationPromptAutoClosed_Success) {
  ShowConfirmationBubbleView(/*iban_saved=*/true, /*hit_max_strikes=*/false);
  task_environment()->FastForwardBy(
      IbanBubbleControllerImpl::kAutoCloseConfirmationBubbleWaitSec);
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
}

// Test that fallback as local save confirmation prompt is not auto-closed in 3
// sec if the IBAN was not successfully saved to the server.
TEST_F(IbanBubbleControllerImplTest, OnConfirmationPromptAutoClosed_Fail) {
  ShowConfirmationBubbleView(/*iban_saved=*/false, /*hit_max_strikes=*/false);
  task_environment()->FastForwardBy(
      IbanBubbleControllerImpl::kAutoCloseConfirmationBubbleWaitSec);
  EXPECT_TRUE(controller()->GetPaymentBubbleView());
}

}  // namespace autofill
