// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_email_dialog_view.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/commerce/core/pref_names.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PriceTrackingEmailDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_params.context = browser()->window()->GetNativeWindow();
    anchor_widget_->Init(std::move(widget_params));

    dialog_coordinator_ = std::make_unique<PriceTrackingEmailDialogCoordinator>(
        anchor_widget_->GetContentsView());

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(GetProfile()), "test@example.com",
        signin::ConsentLevel::kSync);
  }

  void TearDownOnMainThread() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    if (dialog_coordinator_->GetBubble()) {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          dialog_coordinator_->GetBubble()->GetWidget());
      dialog_coordinator_->GetBubble()->GetWidget()->Close();
      destroyed_waiter.Wait();
    }

    dialog_coordinator_.reset();
    anchor_widget_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void CreateAndShowDialog() {
    dialog_coordinator_->Show(browser()->tab_strip_model()->GetWebContentsAt(0),
                              GetProfile(), base::DoNothing());
  }

  PriceTrackingEmailDialogCoordinator* GetCoordinator() {
    return dialog_coordinator_.get();
  }

  PrefService* GetPrefs() { return GetProfile()->GetPrefs(); }

  void SetupDialogAndVerifyInitialState() {
    CreateAndShowDialog();

    EXPECT_FALSE(
        GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
    EXPECT_FALSE(
        GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));
  }

  PriceTrackingEmailDialogView* GetAndVerifyBubble() {
    auto* bubble = GetCoordinator()->GetBubble();
    EXPECT_TRUE(bubble);
    return bubble;
  }

 private:
  views::UniqueWidgetPtr anchor_widget_;
  std::unique_ptr<PriceTrackingEmailDialogCoordinator> dialog_coordinator_;
};

// Accepting the dialog should result in setting the email preference to "true".
IN_PROC_BROWSER_TEST_F(PriceTrackingEmailDialogViewBrowserTest,
                       DialogAccepted) {
  SetupDialogAndVerifyInitialState();

  auto* bubble = GetAndVerifyBubble();
  bubble->Accept();

  EXPECT_TRUE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_TRUE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));
}

// Rejecting the dialog should result in setting the email preference to
// "false".
IN_PROC_BROWSER_TEST_F(PriceTrackingEmailDialogViewBrowserTest,
                       DialogRejected) {
  SetupDialogAndVerifyInitialState();

  auto* bubble = GetAndVerifyBubble();
  bubble->Cancel();

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_TRUE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));
}

// Closing the dialog should result in no state change in preferences.
IN_PROC_BROWSER_TEST_F(PriceTrackingEmailDialogViewBrowserTest, DialogClosed) {
  SetupDialogAndVerifyInitialState();

  auto* bubble = GetAndVerifyBubble();
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble->GetWidget());
  bubble->GetWidget()->CloseNow();
  destroyed_waiter.Wait();

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_FALSE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));
}
