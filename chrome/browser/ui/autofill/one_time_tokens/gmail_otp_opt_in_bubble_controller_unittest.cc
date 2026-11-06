// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/one_time_tokens/gmail_otp_opt_in_bubble_controller.h"

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/bubble_manager_impl.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_result.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

class MockAutofillBubble : public AutofillBubbleBase {
 public:
  MockAutofillBubble() = default;
  ~MockAutofillBubble() override = default;

  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, IsMouseHovered, (), (const, override));
};

class MockOtherBubbleController : public BubbleControllerBase {
 public:
  explicit MockOtherBubbleController(BubbleType bubble_type)
      : bubble_type_(bubble_type) {
    ON_CALL(*this, ShowBubble).WillByDefault([this]() { is_showing_ = true; });
    ON_CALL(*this, HideBubble).WillByDefault([this]() { is_showing_ = false; });
    ON_CALL(*this, IsShowingBubble).WillByDefault([this]() {
      return is_showing_;
    });
    ON_CALL(*this, CanBeReshown).WillByDefault(Return(true));
    ON_CALL(*this, ShouldReshowOnTabVisible).WillByDefault(Return(true));
    ON_CALL(*this, GetBubbleType).WillByDefault(Return(bubble_type_));
    ON_CALL(*this, GetBubbleControllerBaseWeakPtr).WillByDefault([this]() {
      return weak_ptr_factory_.GetWeakPtr();
    });
  }
  ~MockOtherBubbleController() override = default;

  MOCK_METHOD(void, ShowBubble, (), (override));
  MOCK_METHOD(void, HideBubble, (bool), (override));
  MOCK_METHOD(void, OnBubbleDiscarded, (), (override));
  MOCK_METHOD(BubbleType, GetBubbleType, (), (const, override));
  MOCK_METHOD(bool, IsShowingBubble, (), (const, override));
  MOCK_METHOD(bool, IsMouseHovered, (), (const, override));
  MOCK_METHOD(bool, CanBeReshown, (), (const, override));
  MOCK_METHOD(bool, ShouldReshowOnTabVisible, (), (const, override));
  MOCK_METHOD(base::WeakPtr<BubbleControllerBase>,
              GetBubbleControllerBaseWeakPtr,
              (),
              (override));

 private:
  const BubbleType bubble_type_;
  bool is_showing_ = false;
  base::WeakPtrFactory<BubbleControllerBase> weak_ptr_factory_{this};
};

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  explicit FakeTabInterface(TestingProfile* testing_profile)
      : scoped_page_action_controller_(unowned_user_data_host_,
                                       page_action_controller_) {
    if (testing_profile) {
      web_contents_factory_ =
          std::make_unique<content::TestWebContentsFactory>();
      web_contents_ = web_contents_factory_->CreateWebContents(testing_profile);
      tabs::TabLookupFromWebContents::CreateForWebContents(web_contents_, this);
    }
  }
  ~FakeTabInterface() override = default;

  page_actions::MockPageActionController& page_action_controller() {
    return page_action_controller_;
  }

  ui::UnownedUserDataHost& GetUnownedUserDataHost() override {
    return unowned_user_data_host_;
  }
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override {
    return unowned_user_data_host_;
  }

  base::CallbackListSubscription RegisterDidActivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override {
    return activation_callbacks_.Add(cb);
  }

  base::CallbackListSubscription RegisterWillDeactivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override {
    return deactivation_callbacks_.Add(cb);
  }

  base::CallbackListSubscription RegisterWillDiscardContents(
      WillDiscardContentsCallback cb) override {
    return will_discard_contents_callbacks_.Add(cb);
  }

  content::WebContents* GetContents() const override { return web_contents_; }

  void Activate() {
    is_activated_ = true;
    activation_callbacks_.Notify(this);
  }

  void Deactivate() {
    is_activated_ = false;
    deactivation_callbacks_.Notify(this);
  }

  void DiscardContents() {
    content::WebContents* const new_contents =
        web_contents_factory_->CreateWebContents(
            web_contents_->GetBrowserContext());
    will_discard_contents_callbacks_.Notify(this, web_contents_, new_contents);
    web_contents_->RemoveUserData(
        tabs::TabLookupFromWebContents::UserDataKey());
    web_contents_ = new_contents;
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents_, this);
  }

  bool IsActivated() const override { return is_activated_; }

 private:
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  ui::UnownedUserDataHost unowned_user_data_host_;
  NiceMock<page_actions::MockPageActionController> page_action_controller_;
  ui::ScopedUnownedUserData<page_actions::PageActionController>
      scoped_page_action_controller_;
  bool is_activated_ = false;
  base::RepeatingCallbackList<void(TabInterface*)> activation_callbacks_;
  base::RepeatingCallbackList<void(TabInterface*)> deactivation_callbacks_;
  base::RepeatingCallbackList<
      void(TabInterface*, content::WebContents*, content::WebContents*)>
      will_discard_contents_callbacks_;
};

class GmailOtpOptInBubbleControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tab_interface_ = std::make_unique<FakeTabInterface>(&profile_);

    bubble_manager_ = std::make_unique<BubbleManagerImpl>(tab_interface_.get());
    tab_interface_->Activate();

    controller_ =
        std::make_unique<GmailOtpOptInBubbleController>(tab_interface_.get());
    ON_CALL(mock_bubble_, Hide)
        .WillByDefault([weak_controller = controller_->GetWeakPtr()]() {
          if (weak_controller) {
            weak_controller->OnBubbleClosed(GmailOtpOptInResult::kUnknown);
          }
        });
    controller_->SetBubbleViewFactoryForTesting(base::BindRepeating(
        [](AutofillBubbleBase* bubble,
           raw_ptr<content::WebContents>* last_shown_web_contents,
           content::WebContents* web_contents, GmailOtpOptInBubbleController*) {
          *last_shown_web_contents = web_contents;
          return bubble;
        },
        &mock_bubble_, &last_shown_web_contents_));
  }

  void TearDown() override {
    last_shown_web_contents_ = nullptr;
    controller_.reset();
    bubble_manager_.reset();
    tab_interface_.reset();
  }

  GmailOtpOptInBubbleController* controller() { return controller_.get(); }
  FakeTabInterface* tab_interface() { return tab_interface_.get(); }
  page_actions::MockPageActionController& page_action_controller() {
    return tab_interface_->page_action_controller();
  }
  BubbleManager* bubble_manager() { return bubble_manager_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
  std::unique_ptr<BubbleManagerImpl> bubble_manager_;
  NiceMock<MockAutofillBubble> mock_bubble_;
  raw_ptr<content::WebContents> last_shown_web_contents_ = nullptr;
  std::unique_ptr<GmailOtpOptInBubbleController> controller_;
};

TEST_F(GmailOtpOptInBubbleControllerTest, From) {
  EXPECT_EQ(GmailOtpOptInBubbleController::From(*tab_interface()),
            controller());
}

TEST_F(GmailOtpOptInBubbleControllerTest, GetBubbleType) {
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::kGmailOtpOptIn);
}

TEST_F(GmailOtpOptInBubbleControllerTest, GetBubbleControllerBaseWeakPtr) {
  base::WeakPtr<BubbleControllerBase> weak_ptr =
      controller()->GetBubbleControllerBaseWeakPtr();
  EXPECT_EQ(weak_ptr.get(), controller());

  controller_.reset();
  EXPECT_EQ(weak_ptr.get(), nullptr);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       SetUpAndShowBubble_ShowsBubbleImmediately) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_CALL(page_action_controller(), Show(kActionGmailOtpOptIn));

  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());

  EXPECT_TRUE(controller()->IsShowingBubble());
  EXPECT_EQ(controller()->account_email(), u"user@gmail.com");
  histogram_tester.ExpectTotalCount("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                    0);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       SetUpAndShowBubble_WhenAlreadyShowing_DiscardsNewRequest) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<GmailOtpOptInResult> first_future;
  controller()->SetUpAndShowBubble(u"first@gmail.com",
                                   first_future.GetCallback());
  ASSERT_TRUE(controller()->IsShowingBubble());

  base::test::TestFuture<GmailOtpOptInResult> second_future;
  controller()->SetUpAndShowBubble(u"second@gmail.com",
                                   second_future.GetCallback());

  EXPECT_EQ(second_future.Get(), GmailOtpOptInResult::kDiscarded);
  EXPECT_FALSE(first_future.IsReady());
  EXPECT_EQ(controller()->account_email(), u"first@gmail.com");
  histogram_tester.ExpectUniqueSample("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                      GmailOtpOptInResult::kDiscarded, 1);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       HideBubble_HidesViewAndInformsBubbleManager) {
  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());
  ASSERT_TRUE(controller()->IsShowingBubble());

  // Queue a lower-priority bubble while the Gmail OTP opt-in bubble is active.
  NiceMock<MockOtherBubbleController> wallet_controller(
      BubbleType::kWalletablePassConsent);
  bubble_manager()->RequestShowController(wallet_controller,
                                          /*force_show=*/false);
  ASSERT_FALSE(wallet_controller.IsShowingBubble());

  EXPECT_CALL(mock_bubble_, Hide());
  EXPECT_CALL(page_action_controller(), Hide(kActionGmailOtpOptIn));
  EXPECT_CALL(wallet_controller, ShowBubble());
  controller()->HideBubble(/*initiated_by_bubble_manager=*/false);

  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_TRUE(wallet_controller.IsShowingBubble());
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       OnBubbleClosed_HidesPageActionAndInformsBubbleManager) {
  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());
  ASSERT_TRUE(controller()->IsShowingBubble());

  // Queue a lower-priority bubble while the Gmail OTP opt-in bubble is active.
  NiceMock<MockOtherBubbleController> wallet_controller(
      BubbleType::kWalletablePassConsent);
  bubble_manager()->RequestShowController(wallet_controller,
                                          /*force_show=*/false);
  ASSERT_FALSE(wallet_controller.IsShowingBubble());

  EXPECT_CALL(page_action_controller(), Hide(kActionGmailOtpOptIn));
  EXPECT_CALL(wallet_controller, ShowBubble());
  controller()->OnBubbleClosed(GmailOtpOptInResult::kClosed);

  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_TRUE(wallet_controller.IsShowingBubble());
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       Destructor_HidesShowingBubbleAndRunsCallbackWithDiscarded) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  ASSERT_TRUE(controller()->IsShowingBubble());

  EXPECT_CALL(mock_bubble_, Hide());
  EXPECT_CALL(page_action_controller(), Hide(kActionGmailOtpOptIn));
  controller_.reset();
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDiscarded);
  histogram_tester.ExpectUniqueSample("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                      GmailOtpOptInResult::kDiscarded, 1);
}

TEST_F(GmailOtpOptInBubbleControllerTest, IsMouseHovered) {
  EXPECT_FALSE(controller()->IsMouseHovered());

  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());
  ASSERT_TRUE(controller()->IsShowingBubble());

  EXPECT_CALL(mock_bubble_, IsMouseHovered()).WillOnce(Return(true));
  EXPECT_TRUE(controller()->IsMouseHovered());

  EXPECT_CALL(mock_bubble_, IsMouseHovered()).WillOnce(Return(false));
  EXPECT_FALSE(controller()->IsMouseHovered());
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       OnBubbleClosed_RunsCallbackWithResult) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  ASSERT_TRUE(controller()->IsShowingBubble());

  controller()->OnBubbleClosed(GmailOtpOptInResult::kDeclined);

  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDeclined);
  histogram_tester.ExpectUniqueSample("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                      GmailOtpOptInResult::kDeclined, 1);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       OnBubbleClosed_WhenPendingInQueue_DoesNotRunCallbackUntilReshown) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  ASSERT_TRUE(controller()->IsShowingBubble());

  // Deactivating the tab re-queues the bubble in BubbleManager and hides it
  // (which synchronously invokes OnBubbleClosed). Because the bubble is queued,
  // OnBubbleClosed should not run the callback yet.
  EXPECT_CALL(mock_bubble_, Hide());
  tab_interface()->Deactivate();
  ASSERT_FALSE(controller()->IsShowingBubble());
  EXPECT_FALSE(future.IsReady());
  histogram_tester.ExpectTotalCount("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                    0);

  // Reactivating the tab reshows the bubble, and closing it records the result
  // once.
  tab_interface()->Activate();
  ASSERT_TRUE(controller()->IsShowingBubble());
  controller()->OnBubbleClosed(GmailOtpOptInResult::kAccepted);
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kAccepted);
  histogram_tester.ExpectUniqueSample("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                      GmailOtpOptInResult::kAccepted, 1);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       OnLearnMoreLinkClicked_RecordsClickWithoutAffectingResult) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  ASSERT_TRUE(controller()->IsShowingBubble());

  controller()->OnLearnMoreLinkClicked();

  histogram_tester.ExpectUniqueSample(
      "OneTimeTokens.Gmail.OptIn.LearnMoreClicked", true, 1);
  histogram_tester.ExpectTotalCount("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                    0);
  EXPECT_FALSE(future.IsReady());

  controller()->OnBubbleClosed(GmailOtpOptInResult::kDeclined);
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDeclined);
  histogram_tester.ExpectUniqueSample("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                      GmailOtpOptInResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.Gmail.OptIn.LearnMoreClicked", 1);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       Destructor_WhenQueued_RunsCallbackWithDiscarded) {
  base::HistogramTester histogram_tester;
  // Show a higher-priority password bubble so the Gmail OTP opt-in bubble is
  // queued in BubbleManager.
  NiceMock<MockOtherBubbleController> password_controller(
      BubbleType::kPassword);
  bubble_manager()->RequestShowController(password_controller,
                                          /*force_show=*/false);
  ASSERT_TRUE(password_controller.IsShowingBubble());

  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_FALSE(future.IsReady());

  // In TabFeatures, `gmail_otp_opt_in_bubble_controller_` is declared after
  // `autofill_bubble_manager_`, so it is destroyed first on tab teardown.
  controller_.reset();
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDiscarded);
  histogram_tester.ExpectUniqueSample("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                      GmailOtpOptInResult::kDiscarded, 1);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       OnBubbleDiscarded_RunsCallbackWithDiscarded) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  controller()->OnBubbleDiscarded();
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDiscarded);
  controller_.reset();
  histogram_tester.ExpectUniqueSample("OneTimeTokens.Gmail.OptIn.BubbleResult",
                                      GmailOtpOptInResult::kDiscarded, 1);
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       Priority_YieldsToSavePasswordAndSaveCard) {
  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());
  ASSERT_TRUE(controller()->IsShowingBubble());

  // A higher-priority save-card bubble preempts the Gmail OTP opt-in bubble.
  NiceMock<MockOtherBubbleController> card_controller(
      BubbleType::kSaveUpdateCard);
  EXPECT_CALL(mock_bubble_, Hide());
  EXPECT_CALL(card_controller, ShowBubble());
  bubble_manager()->RequestShowController(card_controller,
                                          /*force_show=*/false);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_);

  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_TRUE(card_controller.IsShowingBubble());

  // When the save-card bubble hides, the preempted Gmail OTP opt-in bubble is
  // shown from the queue.
  bubble_manager()->OnBubbleHiddenByController(card_controller,
                                               /*show_next_bubble=*/true);
  EXPECT_TRUE(controller()->IsShowingBubble());
}

TEST_F(GmailOtpOptInBubbleControllerTest, Reshow_ReshowsOnTabActivation) {
  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());
  ASSERT_TRUE(controller()->IsShowingBubble());
  EXPECT_TRUE(controller()->ShouldReshowOnTabVisible());

  EXPECT_CALL(mock_bubble_, Hide());
  tab_interface()->Deactivate();
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_);
  EXPECT_FALSE(controller()->IsShowingBubble());

  tab_interface()->Activate();
  EXPECT_TRUE(controller()->IsShowingBubble());
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       WillDiscardContents_UpdatesObservedWebContents) {
  content::WebContents* const old_contents = tab_interface()->GetContents();
  tab_interface()->DiscardContents();
  content::WebContents* const new_contents = tab_interface()->GetContents();
  ASSERT_NE(new_contents, old_contents);

  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());
  EXPECT_EQ(last_shown_web_contents_, new_contents);
}

}  // namespace
}  // namespace autofill
