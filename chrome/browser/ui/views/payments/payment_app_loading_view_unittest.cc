// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_app_loading_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace payments {

class PaymentAppLoadingViewTest : public ChromeViewsTestBase {
 public:
  PaymentAppLoadingViewTest() = default;
  PaymentAppLoadingViewTest(const PaymentAppLoadingViewTest&) = delete;
  PaymentAppLoadingViewTest& operator=(const PaymentAppLoadingViewTest&) =
      delete;
  ~PaymentAppLoadingViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    loading_view_ =
        widget_->SetContentsView(std::make_unique<PaymentAppLoadingView>(
            &icon_, GURL("https://app.com"), GURL("https://merchant.com"),
            base::BindRepeating([](const ui::Event&) {})));
  }

  void TearDown() override {
    loading_view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget& widget() { return *widget_; }
  PaymentAppLoadingView& loading_view() { return *loading_view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PaymentAppLoadingView> loading_view_ = nullptr;
  SkBitmap icon_;
};

TEST_F(PaymentAppLoadingViewTest, LoadingMessageShownAfterDelay) {
  EXPECT_TRUE(loading_view().GetVisible());
  task_environment()->FastForwardBy(base::Milliseconds(499));
  EXPECT_FALSE(
      loading_view().loading_message_label_for_testing()->GetVisible());

  task_environment()->FastForwardBy(base::Milliseconds(1));

  EXPECT_TRUE(loading_view().loading_message_label_for_testing()->GetVisible());
}

TEST_F(PaymentAppLoadingViewTest, HideBeforeLoadingMessageShown) {
  EXPECT_TRUE(loading_view().GetVisible());
  EXPECT_FALSE(
      loading_view().loading_message_label_for_testing()->GetVisible());

  base::RunLoop run_loop;
  loading_view().Hide(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(PaymentAppLoadingViewTest, HideWithMinimumLoadingMessageDuration) {
  EXPECT_TRUE(loading_view().GetVisible());
  task_environment()->FastForwardBy(base::Milliseconds(499));
  EXPECT_FALSE(
      loading_view().loading_message_label_for_testing()->GetVisible());

  // Fast forward until the loading message is shown.
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(loading_view().loading_message_label_for_testing()->GetVisible());

  base::MockOnceClosure hidden_callback;
  // Call Hide(). The callback should not run because the minimum display
  // duration has not elapsed.
  EXPECT_CALL(hidden_callback, Run).Times(0);
  loading_view().Hide(hidden_callback.Get());

  // Fast forward by 499ms. The callback should not run yet because minimum
  // display duration (500ms) has not elapsed.
  task_environment()->FastForwardBy(base::Milliseconds(499));
  testing::Mock::VerifyAndClearExpectations(&hidden_callback);

  // Fast forward by remaining 1ms. The callback should run now.
  EXPECT_CALL(hidden_callback, Run);
  task_environment()->FastForwardBy(base::Milliseconds(1));
}

}  // namespace payments
