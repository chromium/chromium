// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {

class TestBubbleManager : public BubbleManager {
 public:
  TestBubbleManager() = default;
  ~TestBubbleManager() override = default;

  // BubbleManager:
  void RequestShowController(autofill::BubbleControllerBase& controller_to_show,
                             bool force_show) override {
    controller_to_show.ShowBubble();
  }
  void OnBubbleHiddenByController(
      autofill::BubbleControllerBase& controller_to_hide,
      bool show_next_bubble) override {}
  bool HasPendingBubbleOfSameType(
      const autofill::BubbleType bubble_type) const override {
    return false;
  }
  bool HasConflictingPendingBubble(
      const autofill::BubbleType bubble_type) const override {
    return false;
  }
};

class IbanBubbleControllerImplTest : public ChromeRenderViewHostTestHarness {
 public:
  explicit IbanBubbleControllerImplTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : ChromeRenderViewHostTestHarness(time_source) {}
  IbanBubbleControllerImplTest(IbanBubbleControllerImplTest&) = delete;
  IbanBubbleControllerImplTest& operator=(IbanBubbleControllerImplTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));

    ON_CALL(mock_tab_interface_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(mock_tab_interface_, IsActivated())
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_tab_interface_, GetTabFeatures())
        .WillByDefault(testing::Return(&tab_features_));
    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&mock_browser_window_interface_));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    tab_features_.SetBubbleManagerForTesting(
        std::make_unique<TestBubbleManager>());

    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_interface_);

    test_autofill_bubble_handler_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<AutofillBubbleHandler>>(
            mock_browser_window_interface_.GetUnownedUserDataHost(),
            test_autofill_bubble_handler_);

    IbanBubbleControllerImpl::CreateForWebContents(web_contents());
  }

  void ShowLocalSaveBubble(const Iban& iban) {
    controller()->OfferLocalSave(
        iban, /*should_show_prompt=*/true,
        base::BindOnce(&IbanBubbleControllerImplTest::SaveIbanCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void ShowUploadSaveBubble(const Iban& iban) {
    LegalMessageLines legal_message = {
        TestLegalMessageLine("google_test_legal_message")};
    controller()->OfferUploadSave(
        iban, legal_message, /*should_show_prompt=*/true,
        base::BindOnce(&IbanBubbleControllerImplTest::SaveIbanCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void ClickSaveButton(const std::u16string& nickname) {
    controller()->OnAcceptButton(nickname);
    controller()->OnBubbleClosed(PaymentsUiClosedReason::kAccepted);
    if (controller()->ShouldShowPaymentSavedLabelAnimation()) {
      controller()->OnAnimationEnded();
    }
  }

  void ShowConfirmationBubbleView(bool iban_saved, bool hit_max_strikes) {
    controller()->ShowConfirmationBubbleView(iban_saved, hit_max_strikes);
  }

  void CloseBubble(PaymentsUiClosedReason closed_reason) {
    controller()->OnBubbleClosed(closed_reason);
  }

  std::u16string_view saved_nickname() { return saved_nickname_; }

 protected:
  IbanBubbleControllerImpl* controller() {
    return IbanBubbleControllerImpl::FromWebContents(web_contents());
  }

 private:
  void SaveIbanCallback(
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname) {
    saved_nickname_ = nickname;
  }

  std::u16string_view saved_nickname_;
  tabs::MockTabInterface mock_tab_interface_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  tabs::TabFeatures tab_features_;
  TestAutofillBubbleHandler test_autofill_bubble_handler_;
  std::unique_ptr<ui::ScopedUnownedUserData<AutofillBubbleHandler>>
      test_autofill_bubble_handler_registration_;
  base::WeakPtrFactory<IbanBubbleControllerImplTest> weak_ptr_factory_{this};
};

TEST_F(IbanBubbleControllerImplTest, LocalIbanSavedSuccessfully) {
  std::u16string nickname = u"My doctor's IBAN";
  ShowLocalSaveBubble(test::GetLocalIban());
  ClickSaveButton(nickname);

  EXPECT_EQ(nickname, saved_nickname());
}

TEST_F(IbanBubbleControllerImplTest, UploadIbanSavedSuccessfully) {
  std::u16string nickname = u"My doctor's IBAN";
  ShowUploadSaveBubble(test::GetServerIban());
  ClickSaveButton(nickname);

  EXPECT_EQ(nickname, saved_nickname());
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanOffered) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(test::GetLocalIban());

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_Accepted) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(test::GetLocalIban());
  CloseBubble(PaymentsUiClosedReason::kAccepted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Local.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kAccepted, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_Cancelled) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(test::GetLocalIban());
  CloseBubble(PaymentsUiClosedReason::kCancelled);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Local.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kCancelled, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_NotInteracted) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(test::GetLocalIban());
  CloseBubble(PaymentsUiClosedReason::kNotInteracted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Local.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kNotInteracted, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanResult_LostFocus) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(test::GetLocalIban());
  CloseBubble(PaymentsUiClosedReason::kLostFocus);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Local.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kLostFocus, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanSaved_WithNickname) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(test::GetLocalIban());
  ClickSaveButton(u"My doctor's IBAN");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", true, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_LocalIbanSaved_NoNickname) {
  base::HistogramTester histogram_tester;
  ShowLocalSaveBubble(test::GetLocalIban());
  ClickSaveButton(u"");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", false, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_UploadIbanOffered) {
  base::HistogramTester histogram_tester;
  ShowUploadSaveBubble(test::GetServerIban());

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Upload.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_UploadIbanResult_Accepted) {
  base::HistogramTester histogram_tester;
  ShowUploadSaveBubble(test::GetServerIban());
  CloseBubble(PaymentsUiClosedReason::kAccepted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Upload.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kAccepted, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_UploadIbanResult_Cancelled) {
  base::HistogramTester histogram_tester;
  ShowUploadSaveBubble(test::GetServerIban());
  CloseBubble(PaymentsUiClosedReason::kCancelled);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Upload.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kCancelled, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_UploadIbanResult_NotInteracted) {
  base::HistogramTester histogram_tester;
  ShowUploadSaveBubble(test::GetServerIban());
  CloseBubble(PaymentsUiClosedReason::kNotInteracted);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Upload.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kNotInteracted, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_UploadIbanResult_LostFocus) {
  base::HistogramTester histogram_tester;
  ShowUploadSaveBubble(test::GetServerIban());
  CloseBubble(PaymentsUiClosedReason::kLostFocus);

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptResult2.Upload.FirstShow",
      autofill_metrics::SaveIbanPromptResult::kLostFocus, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_UploadIbanSaved_WithNickname) {
  base::HistogramTester histogram_tester;
  ShowUploadSaveBubble(test::GetServerIban());
  ClickSaveButton(u"My doctor's IBAN");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Upload.SavedWithNickname", true, 1);
}

TEST_F(IbanBubbleControllerImplTest, Metrics_UploadIbanSaved_NoNickname) {
  base::HistogramTester histogram_tester;
  ShowUploadSaveBubble(test::GetServerIban());
  ClickSaveButton(u"");

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Upload.SavedWithNickname", false, 1);
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

TEST_F(IbanBubbleControllerImplTest, ReturnsApplicableExplanatoryMessage) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableWalletBranding};
  ShowUploadSaveBubble(test::GetServerIban());
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPLOAD_IBAN_TO_WALLET_PROMPT_EXPLANATION));
}

TEST_F(IbanBubbleControllerImplTest,
       ReturnsApplicableExplanatoryMessage_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillEnableWalletBranding);
  ShowUploadSaveBubble(test::GetServerIban());
  EXPECT_EQ(
      controller()->GetExplanatoryMessage(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_UPLOAD_IBAN_PROMPT_EXPLANATION));
}

TEST_F(IbanBubbleControllerImplTest, ReturnsApplicableWindowTitle) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableWalletBrandingV2};
  ShowUploadSaveBubble(test::GetServerIban());
  EXPECT_EQ(
      controller()->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_TO_WALLET_PROMPT_TITLE));
}

TEST_F(IbanBubbleControllerImplTest,
       ReturnsApplicableWindowTitle_WalletBrandingV2Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillEnableWalletBrandingV2);
  ShowUploadSaveBubble(test::GetServerIban());
  EXPECT_EQ(
      controller()->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_SERVER));
}

}  // namespace autofill
