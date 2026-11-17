// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"

#include "base/callback_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/bubble_manager_impl.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

// Test `AutofillBubbleBase` implementation which notifies the controller when
// the bubble hides (to match prod).
class ObserveHideTestAutofillBubble : public AutofillBubbleBase {
 public:
  explicit ObserveHideTestAutofillBubble(content::WebContents* web_contents)
      : web_contents_(web_contents->GetWeakPtr()) {}
  ~ObserveHideTestAutofillBubble() override = default;

  void Hide() override {
    // Call `OnBubbleClosed()` because the real implementation does so.
    if (web_contents_) {
      IbanBubbleControllerImpl* const controller =
          IbanBubbleControllerImpl::FromWebContents(web_contents_.get());
      controller->OnBubbleClosed(PaymentsUiClosedReason::kUnknown);
    }
  }

  bool IsMouseHovered() const override { return false; }

 private:
  // `WeakPtr` because `ObserveHideTestAutofillBubble` outlives the
  // `WebContents` in tests.
  const base::WeakPtr<content::WebContents> web_contents_;
};

class ObserveHideTestAutofillBubbleHandler : public TestAutofillBubbleHandler {
 public:
  ObserveHideTestAutofillBubbleHandler() = default;
  ObserveHideTestAutofillBubbleHandler(
      const ObserveHideTestAutofillBubbleHandler&) = delete;
  ObserveHideTestAutofillBubbleHandler& operator=(
      const ObserveHideTestAutofillBubbleHandler&) = delete;
  ~ObserveHideTestAutofillBubbleHandler() override = default;

  AutofillBubbleBase* ShowIbanBubble(content::WebContents* web_contents,
                                     IbanBubbleController* controller,
                                     bool is_user_gesture,
                                     IbanBubbleType bubble_type) override {
    if (!iban_bubble_) {
      iban_bubble_ =
          std::make_unique<ObserveHideTestAutofillBubble>(web_contents);
    }
    return iban_bubble_.get();
  }

  AutofillBubbleBase* ShowSaveIbanConfirmationBubble(
      content::WebContents* web_contents,
      IbanBubbleController* controller) override {
    if (!confirmation_bubble_) {
      confirmation_bubble_ =
          std::make_unique<ObserveHideTestAutofillBubble>(web_contents);
    }
    return confirmation_bubble_.get();
  }

 private:
  std::unique_ptr<ObserveHideTestAutofillBubble> iban_bubble_;
  std::unique_ptr<ObserveHideTestAutofillBubble> confirmation_bubble_;
};

}  // namespace

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
        .WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_interface_, IsActivated()).WillByDefault(Return(true));
    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(mock_tab_interface_, RegisterDidActivate)
        .WillByDefault([this](tabs::TabInterface::DidActivateCallback cb) {
          return did_activate_callbacks_.Add(std::move(cb));
        });
    ON_CALL(mock_tab_interface_, RegisterWillDeactivate)
        .WillByDefault([this](tabs::TabInterface::WillDeactivateCallback cb) {
          return will_deactivate_callbacks_.Add(std::move(cb));
        });
    ON_CALL(mock_tab_interface_, RegisterWillDiscardContents)
        .WillByDefault(
            [this](tabs::TabInterface::WillDiscardContentsCallback cb) {
              return will_discard_callbacks_.Add(std::move(cb));
            });
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    bubble_manager_ = std::make_unique<BubbleManagerImpl>(&mock_tab_interface_);

    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_interface_);

    test_autofill_bubble_handler_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<AutofillBubbleHandler>>(
            mock_browser_window_interface_.GetUnownedUserDataHost(),
            test_autofill_bubble_handler_);

    IbanBubbleControllerImpl::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    bubble_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetTabActive(bool active) {
    ON_CALL(mock_tab_interface_, IsActivated()).WillByDefault(Return(active));
    if (active) {
      did_activate_callbacks_.Notify(&mock_tab_interface_);
    } else {
      will_deactivate_callbacks_.Notify(&mock_tab_interface_);
    }
  }

  BubbleManager& bubble_manager() { return *bubble_manager_; }

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
  base::RepeatingCallbackList<void(tabs::TabInterface*)>
      did_activate_callbacks_;
  base::RepeatingCallbackList<void(tabs::TabInterface*)>
      will_deactivate_callbacks_;
  base::RepeatingCallbackList<
      void(tabs::TabInterface*, content::WebContents*, content::WebContents*)>
      will_discard_callbacks_;
  std::unique_ptr<BubbleManagerImpl> bubble_manager_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  ObserveHideTestAutofillBubbleHandler test_autofill_bubble_handler_;
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
  EXPECT_EQ(controller()->GetIbanBubbleType(), IbanBubbleType::kInactive);
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

TEST_F(IbanBubbleControllerImplTest,
       NonTabWebContentsDoesNotCrashOrShowBubble) {
  web_contents()->RemoveUserData(tabs::TabLookupFromWebContents::UserDataKey());
  ASSERT_EQ(tabs::TabInterface::MaybeGetFromContents(web_contents()), nullptr);

  base::HistogramTester histogram_tester;
  for (bool should_show_prompt : {true, false}) {
    controller()->OfferLocalSave(test::GetLocalIban(), should_show_prompt,
                                 base::DoNothing());
    EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
    EXPECT_EQ(controller()->GetIbanBubbleType(), IbanBubbleType::kInactive);
    EXPECT_FALSE(controller()->IsIconVisible());

    controller()->OfferUploadSave(test::GetServerIban(), LegalMessageLines(),
                                  should_show_prompt, base::DoNothing());
    EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
    EXPECT_EQ(controller()->GetIbanBubbleType(), IbanBubbleType::kInactive);
    EXPECT_FALSE(controller()->IsIconVisible());
  }

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveIbanPromptOffer.Upload.FirstShow", 0);
}

// Tests that an upload confirmation arriving while the tab is inactive is
// queued in `BubbleManager` and starts its auto-close timer only once shown.
TEST_F(IbanBubbleControllerImplTest,
       ShowConfirmationBubbleView_QueuedWhileInactiveAndAutoClosesOnShow) {
  SetTabActive(false);
  ShowConfirmationBubbleView(/*iban_saved=*/true, /*hit_max_strikes=*/false);

  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_TRUE(
      bubble_manager().HasPendingBubbleOfSameType(BubbleType::kSaveIban));

  // Auto-close timer must not run while the confirmation bubble is queued.
  task_environment()->FastForwardBy(
      IbanBubbleControllerImpl::kAutoCloseConfirmationBubbleWaitSec);
  EXPECT_TRUE(
      bubble_manager().HasPendingBubbleOfSameType(BubbleType::kSaveIban));

  // Activating the tab shows the queued confirmation bubble and starts the
  // auto-close timer.
  SetTabActive(true);
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);

  task_environment()->FastForwardBy(
      IbanBubbleControllerImpl::kAutoCloseConfirmationBubbleWaitSec);
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_EQ(controller()->GetIbanBubbleType(), IbanBubbleType::kInactive);
}

// Tests that closing a confirmation bubble early stops its auto-close timer so
// a subsequent save bubble is not closed when the original timer elapses.
TEST_F(IbanBubbleControllerImplTest,
       EarlyConfirmationCloseStopsAutoCloseTimer) {
  ShowConfirmationBubbleView(/*iban_saved=*/true, /*hit_max_strikes=*/false);
  ASSERT_NE(controller()->GetPaymentBubbleView(), nullptr);

  SetTabActive(false);
  SetTabActive(true);
  ASSERT_EQ(controller()->GetPaymentBubbleView(), nullptr);
  ASSERT_EQ(controller()->GetIbanBubbleType(), IbanBubbleType::kInactive);

  ShowLocalSaveBubble(test::GetLocalIban());
  ASSERT_NE(controller()->GetPaymentBubbleView(), nullptr);

  task_environment()->FastForwardBy(
      IbanBubbleControllerImpl::kAutoCloseConfirmationBubbleWaitSec);
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_EQ(controller()->GetIbanBubbleType(), IbanBubbleType::kLocalSave);
}

// Tests that discarding a queued upload confirmation resets confirmation state.
TEST_F(IbanBubbleControllerImplTest,
       ShowConfirmationBubbleView_DiscardedWhileQueuedResetsState) {
  SetTabActive(false);
  ShowConfirmationBubbleView(/*iban_saved=*/true, /*hit_max_strikes=*/false);
  ASSERT_EQ(controller()->GetIbanBubbleType(),
            IbanBubbleType::kUploadCompleted);

  // Exceed `BubbleManagerImpl`'s pending request timeout (1 hour) so the queued
  // confirmation bubble is discarded when the tab becomes active.
  task_environment()->FastForwardBy(base::Hours(2));
  SetTabActive(true);

  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_EQ(controller()->GetIbanBubbleType(), IbanBubbleType::kInactive);
  EXPECT_FALSE(controller()->IsIconVisible());
}

}  // namespace autofill
