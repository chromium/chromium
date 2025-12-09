// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/wallet/core/browser/metrics/wallet_metrics.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/layout_provider.h"

namespace wallet {
namespace {

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

class TestWalletablePassConsentBubbleController
    : public WalletablePassConsentBubbleController {
 public:
  explicit TestWalletablePassConsentBubbleController(tabs::TabInterface* tab)
      : WalletablePassConsentBubbleController(tab) {}
  ~TestWalletablePassConsentBubbleController() override = default;

  void ShowBubble() override { SetBubbleView(*test_bubble_view_); }

  void SetTestBubbleView(WalletablePassBubbleViewBase* test_bubble_view) {
    test_bubble_view_ = test_bubble_view;
  }

 private:
  raw_ptr<WalletablePassBubbleViewBase> test_bubble_view_ = nullptr;
};

}  // namespace

class WalletablePassConsentBubbleControllerTest : public ::testing::Test {
 public:
  WalletablePassConsentBubbleControllerTest() {
    tab_interface_ = std::make_unique<FakeTabInterface>(&profile_);
    controller_ = std::make_unique<TestWalletablePassConsentBubbleController>(
        tab_interface_.get());
    test_bubble_view_ = std::make_unique<WalletablePassBubbleViewBase>(
        nullptr, tab_interface_->GetContents(), controller_.get());
    controller_->SetTestBubbleView(test_bubble_view_.get());
  }

 protected:
  TestWalletablePassConsentBubbleController* controller() const {
    return controller_.get();
  }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  // This initializes the global LayoutProvider singleton required by the View's
  // constructor.
  views::LayoutProvider layout_provider_;
  TestingProfile profile_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
  std::unique_ptr<WalletablePassBubbleViewBase> test_bubble_view_;
  std::unique_ptr<TestWalletablePassConsentBubbleController> controller_;
};

TEST_F(WalletablePassConsentBubbleControllerTest, LogLearnMoreClicked) {
  base::test::TestFuture<WalletablePassClient::WalletablePassBubbleResult>
      future;
  controller()->SetUpAndShowConsentBubble(PassCategory::kBoardingPass,
                                          future.GetCallback());
  controller()->OnLearnMoreClicked();
  histogram_tester_.ExpectUniqueSample(
      "Wallet.WalletablePass.OptIn.Funnel.BoardingPass",
      metrics::WalletablePassOptInFunnelEvents::kLearnMoreButtonClicked, 1);
}

}  // namespace wallet
