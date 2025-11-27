// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

using WalletablePassBubbleResult =
    WalletablePassClient::WalletablePassBubbleResult;
using WalletablePassBubbleClosedReason =
    WalletablePassBubbleControllerBase::WalletablePassBubbleClosedReason;

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  ~FakeTabInterface() override = default;
  explicit FakeTabInterface(TestingProfile* testing_profile);
  content::WebContents* GetContents() const override { return web_contents_; }

 private:
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  // Owned by `web_contents_factory_`.
  raw_ptr<content::WebContents> web_contents_;
};

FakeTabInterface::FakeTabInterface(TestingProfile* testing_profile) {
  if (testing_profile) {
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    web_contents_ = web_contents_factory_->CreateWebContents(testing_profile);
  }
}

class TestWalletablePassSaveBubbleController
    : public WalletablePassSaveBubbleController {
 public:
  explicit TestWalletablePassSaveBubbleController(tabs::TabInterface* tab)
      : WalletablePassSaveBubbleController(tab) {}
  ~TestWalletablePassSaveBubbleController() override = default;

  void ShowBubble() override { SetBubbleView(*test_bubble_view_); }

  void SetTestBubbleView(WalletablePassBubbleViewBase* test_bubble_view) {
    test_bubble_view_ = test_bubble_view;
  }

 private:
  raw_ptr<WalletablePassBubbleViewBase> test_bubble_view_ = nullptr;
};

}  // namespace

class WalletablePassSaveBubbleControllerTest : public ::testing::Test {
 public:
  WalletablePassSaveBubbleControllerTest() {
    tab_interface_ = std::make_unique<FakeTabInterface>(&profile_);
    controller_ = std::make_unique<TestWalletablePassSaveBubbleController>(
        tab_interface_.get());
    test_bubble_view_ = std::make_unique<WalletablePassBubbleViewBase>(
        nullptr, tab_interface_->GetContents(), controller_.get());
    controller_->SetTestBubbleView(test_bubble_view_.get());
  }

 protected:
  TestWalletablePassSaveBubbleController* controller() const {
    return controller_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  // This initializes the global LayoutProvider singleton required by the View's
  // constructor.
  views::LayoutProvider layout_provider_;
  TestingProfile profile_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
  std::unique_ptr<WalletablePassBubbleViewBase> test_bubble_view_;
  std::unique_ptr<TestWalletablePassSaveBubbleController> controller_;
};

// Tests that the callback is run with kAccepted when the bubble is accepted.
TEST_F(WalletablePassSaveBubbleControllerTest, Accepted) {
  base::test::TestFuture<WalletablePassBubbleResult> future;

  controller()->SetUpAndShowSaveBubble({}, future.GetCallback());
  EXPECT_TRUE(controller()->IsShowingBubble());

  controller()->OnBubbleClosed(WalletablePassBubbleClosedReason::kAccepted);
  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_EQ(future.Get(), WalletablePassBubbleResult::kAccepted);
}

// Tests that the callback is run with kDeclined when the bubble is declined.
TEST_F(WalletablePassSaveBubbleControllerTest, Declined) {
  base::test::TestFuture<WalletablePassBubbleResult> future;

  controller()->SetUpAndShowSaveBubble({}, future.GetCallback());
  EXPECT_TRUE(controller()->IsShowingBubble());

  controller()->OnBubbleClosed(WalletablePassBubbleClosedReason::kDeclined);
  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_EQ(future.Get(), WalletablePassBubbleResult::kDeclined);
}

// Tests that the callback is run with kClosed when the bubble is closed.
TEST_F(WalletablePassSaveBubbleControllerTest, Closed) {
  base::test::TestFuture<WalletablePassBubbleResult> future;

  controller()->SetUpAndShowSaveBubble({}, future.GetCallback());
  EXPECT_TRUE(controller()->IsShowingBubble());

  controller()->OnBubbleClosed(WalletablePassBubbleClosedReason::kClosed);
  EXPECT_FALSE(controller()->IsShowingBubble());
  EXPECT_EQ(future.Get(), WalletablePassBubbleResult::kClosed);
}

}  // namespace wallet
