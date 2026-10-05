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
#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/bubble_manager_impl.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::Return;
using GmailOtpOptInResult = GmailOtpOptInBubbleController::GmailOtpOptInResult;

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
  FakeTabInterface(TestingProfile* testing_profile,
                   tabs::TabFeatures* tab_features)
      : tab_features_(tab_features) {
    if (testing_profile) {
      web_contents_factory_ =
          std::make_unique<content::TestWebContentsFactory>();
      web_contents_ = web_contents_factory_->CreateWebContents(testing_profile);
    }
  }
  ~FakeTabInterface() override = default;

  tabs::TabFeatures* GetTabFeatures() override { return tab_features_; }
  const tabs::TabFeatures* GetTabFeatures() const override {
    return tab_features_;
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

  content::WebContents* GetContents() const override { return web_contents_; }

  void Activate() {
    is_activated_ = true;
    activation_callbacks_.Notify(this);
  }

  void Deactivate() {
    is_activated_ = false;
    deactivation_callbacks_.Notify(this);
  }

  bool IsActivated() const override { return is_activated_; }

 private:
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<tabs::TabFeatures> tab_features_ = nullptr;
  ui::UnownedUserDataHost unowned_user_data_host_;
  bool is_activated_ = false;
  base::RepeatingCallbackList<void(TabInterface*)> activation_callbacks_;
  base::RepeatingCallbackList<void(TabInterface*)> deactivation_callbacks_;
};

class GmailOtpOptInBubbleControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tab_interface_ =
        std::make_unique<FakeTabInterface>(&profile_, &tab_features_);

    tab_features_.SetBubbleManagerForTesting(
        std::make_unique<BubbleManagerImpl>(tab_interface_.get()));
    tab_interface_->Activate();

    controller_ =
        std::make_unique<GmailOtpOptInBubbleController>(*tab_interface_);
    controller_->SetBubbleViewFactoryForTesting(base::BindRepeating(
        [](AutofillBubbleBase* bubble, content::WebContents*,
           GmailOtpOptInBubbleController*) { return bubble; },
        &mock_bubble_));
  }

  void TearDown() override {
    controller_.reset();
    tab_features_.SetBubbleManagerForTesting(nullptr);
    tab_interface_.reset();
  }

  GmailOtpOptInBubbleController* controller() { return controller_.get(); }
  FakeTabInterface* tab_interface() { return tab_interface_.get(); }
  BubbleManager* bubble_manager() {
    return tab_features_.autofill_bubble_manager();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  tabs::TabFeatures tab_features_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
  NiceMock<MockAutofillBubble> mock_bubble_;
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
  EXPECT_FALSE(controller()->IsShowingBubble());

  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());

  EXPECT_TRUE(controller()->IsShowingBubble());
  EXPECT_EQ(controller()->account_email(), u"user@gmail.com");
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       SetUpAndShowBubble_WhenAlreadyShowing_DiscardsNewRequest) {
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
  EXPECT_CALL(wallet_controller, ShowBubble());
  controller()->HideBubble(/*initiated_by_bubble_manager=*/false);

  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_TRUE(wallet_controller.IsShowingBubble());
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       HideBubble_InitiatedByBubbleManagerDoesNotInformBubbleManager) {
  controller()->SetUpAndShowBubble(u"user@gmail.com", base::DoNothing());
  ASSERT_TRUE(controller()->IsShowingBubble());

  // Queue a lower-priority bubble while the Gmail OTP opt-in bubble is active.
  NiceMock<MockOtherBubbleController> wallet_controller(
      BubbleType::kWalletablePassConsent);
  bubble_manager()->RequestShowController(wallet_controller,
                                          /*force_show=*/false);
  ASSERT_FALSE(wallet_controller.IsShowingBubble());

  EXPECT_CALL(mock_bubble_, Hide());
  EXPECT_CALL(wallet_controller, ShowBubble()).Times(0);
  controller()->HideBubble(/*initiated_by_bubble_manager=*/true);

  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_FALSE(wallet_controller.IsShowingBubble());
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       Destructor_HidesShowingBubbleAndRunsCallbackWithDiscarded) {
  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  ASSERT_TRUE(controller()->IsShowingBubble());

  EXPECT_CALL(mock_bubble_, Hide());
  controller_.reset();
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDiscarded);
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
       Destructor_WhenQueued_RunsCallbackWithDiscarded) {
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
}

TEST_F(GmailOtpOptInBubbleControllerTest,
       OnBubbleDiscarded_RunsCallbackWithDiscarded) {
  base::test::TestFuture<GmailOtpOptInResult> future;
  controller()->SetUpAndShowBubble(u"user@gmail.com", future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  controller()->OnBubbleDiscarded();
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDiscarded);
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

}  // namespace
}  // namespace autofill
