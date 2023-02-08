// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_iban_bubble_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestSaveIbanBubbleControllerImpl : public SaveIbanBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestSaveIbanBubbleControllerImpl>(web_contents));
  }

  explicit TestSaveIbanBubbleControllerImpl(content::WebContents* web_contents)
      : SaveIbanBubbleControllerImpl(web_contents) {}
};

class SaveIbanBubbleControllerImplTest : public BrowserWithTestWindowTest {
 public:
  SaveIbanBubbleControllerImplTest() = default;
  SaveIbanBubbleControllerImplTest(SaveIbanBubbleControllerImplTest&) = delete;
  SaveIbanBubbleControllerImplTest& operator=(
      SaveIbanBubbleControllerImplTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestSaveIbanBubbleControllerImpl::CreateForTesting(web_contents);
  }

  void ShowLocalBubble(const IBAN& iban) {
    controller()->OfferLocalSave(
        iban, /*should_show_prompt=*/true,
        base::BindOnce(&SaveIbanBubbleControllerImplTest::LocalSaveIBANCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void ClickSaveButton(const std::u16string& nickname) {
    controller()->OnSaveButton(nickname);
    controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kAccepted);
    if (controller()->ShouldShowPaymentSavedLabelAnimation()) {
      controller()->OnAnimationEnded();
    }
  }

  void CloseBubble(PaymentsBubbleClosedReason closed_reason) {
    controller()->OnBubbleClosed(closed_reason);
  }

  std::u16string saved_nickname() { return saved_nickname_; }

 protected:
  TestSaveIbanBubbleControllerImpl* controller() {
    return static_cast<TestSaveIbanBubbleControllerImpl*>(
        TestSaveIbanBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  void LocalSaveIBANCallback(
      AutofillClient::SaveIBANOfferUserDecision user_decision,
      const absl::optional<std::u16string>& nickname) {
    saved_nickname_ = nickname.value_or(u"");
  }

  std::u16string saved_nickname_;
  base::WeakPtrFactory<SaveIbanBubbleControllerImplTest> weak_ptr_factory_{
      this};
};

TEST_F(SaveIbanBubbleControllerImplTest, LocalIbanSavedSuccessfully) {
  std::u16string nickname = u"My doctor's IBAN";
  ShowLocalBubble(autofill::test::GetIBAN());
  ClickSaveButton(nickname);

  EXPECT_EQ(nickname, saved_nickname());
}

TEST_F(SaveIbanBubbleControllerImplTest, Metrics_LocalIbanOffered) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(autofill::test::GetIBAN());

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
}

TEST_F(SaveIbanBubbleControllerImplTest, Metrics_LocalIbanResult_Accepted) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(autofill::test::GetIBAN());
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kAccepted, 1);
}

TEST_F(SaveIbanBubbleControllerImplTest, Metrics_LocalIbanResult_Cancelled) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(autofill::test::GetIBAN());
  CloseBubble(PaymentsBubbleClosedReason::kCancelled);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kCancelled, 1);
}

TEST_F(SaveIbanBubbleControllerImplTest,
       Metrics_LocalIbanResult_NotInteracted) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(autofill::test::GetIBAN());
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kNotInteracted, 1);
}

TEST_F(SaveIbanBubbleControllerImplTest, Metrics_LocalIbanResult_LostFocus) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(autofill::test::GetIBAN());
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kLostFocus, 1);
}

TEST_F(SaveIbanBubbleControllerImplTest, Metrics_LocalIbanSaved_WithNickname) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(autofill::test::GetIBAN());
  ClickSaveButton(u"My doctor's IBAN");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", true, 1);
}

TEST_F(SaveIbanBubbleControllerImplTest, Metrics_LocalIbanSaved_NoNickname) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(autofill::test::GetIBAN());
  ClickSaveButton(u"");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", false, 1);
}

}  // namespace autofill
