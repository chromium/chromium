// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_email_dialog_view.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/commerce/core/pref_names.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PriceTrackingEmailDialogViewUnitTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
    widget_params.context = GetContext();
    anchor_widget_->Init(std::move(widget_params));

    dialog_coordinator_ = std::make_unique<PriceTrackingEmailDialogCoordinator>(
        anchor_widget_->GetContentsView());

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile()), "test@example.com",
        signin::ConsentLevel::kSync);
  }

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    if (dialog_coordinator_->GetBubble()) {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          dialog_coordinator_->GetBubble()->GetWidget());
      dialog_coordinator_->GetBubble()->GetWidget()->Close();
      destroyed_waiter.Wait();
    }

    dialog_coordinator_.reset();
    anchor_widget_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void CreateAndShowDialog() {
    dialog_coordinator_->Show(browser()->tab_strip_model()->GetWebContentsAt(0),
                              profile(), base::DoNothing());
  }

  PriceTrackingEmailDialogCoordinator* GetCoordinator() {
    return dialog_coordinator_.get();
  }

  PrefService* GetPrefs() { return profile()->GetPrefs(); }

 private:
  views::UniqueWidgetPtr anchor_widget_;
  std::unique_ptr<PriceTrackingEmailDialogCoordinator> dialog_coordinator_;
};

// Accpting the dialog should result in setting the email preference to "true".
TEST_F(PriceTrackingEmailDialogViewUnitTest, DialogAccepted) {
  CreateAndShowDialog();

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_FALSE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));

  auto* bubble = GetCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  bubble->Accept();

  EXPECT_TRUE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_TRUE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));
}

// Rejecting the dialog should result in setting the email preference to
// "false".
TEST_F(PriceTrackingEmailDialogViewUnitTest, DialogRejected) {
  CreateAndShowDialog();

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_FALSE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));

  auto* bubble = GetCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  bubble->Cancel();

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_TRUE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));
}

// Closing the dialog should result in no state change in preferences.
TEST_F(PriceTrackingEmailDialogViewUnitTest, DialogClosed) {
  CreateAndShowDialog();

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_FALSE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));

  auto* bubble = GetCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  bubble->Close();

  EXPECT_FALSE(
      GetPrefs()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  EXPECT_FALSE(
      GetPrefs()->HasPrefPath(commerce::kPriceEmailNotificationsEnabled));
}
