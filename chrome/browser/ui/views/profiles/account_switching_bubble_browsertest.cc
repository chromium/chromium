// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/account_switching_bubble.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/account_switching_manager.h"
#include "chrome/browser/signin/account_switching_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/data_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kBubbleWidgetName[] = "AccountSwitchingBubbleViews";
constexpr char kPrimaryEmail[] = "primary@gmail.com";
constexpr char16_t kPrimaryEmail16[] = u"primary@gmail.com";
constexpr char kTargetEmail[] = "target@gmail.com";
constexpr char16_t kTargetEmail16[] = u"target@gmail.com";
constexpr char kThirdEmail[] = "third@gmail.com";

std::unique_ptr<KeyedService> CreateTestAccountPreviewDataService(
    content::BrowserContext*) {
  return std::make_unique<signin::TestAccountPreviewDataService>();
}

class AccountSwitchingBubbleBrowserTest : public SigninBrowserTestBase {
 public:
  AccountSwitchingBubbleBrowserTest() {
    feature_list_.InitWithFeatures(
        {switches::kEnableAccountPreviewData,
         switches::kEnableAccountPreviewSwitchingAccount},
        {});
  }

  ~AccountSwitchingBubbleBrowserTest() override = default;

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    AccountPreviewDataServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestAccountPreviewDataService));
  }

  void SetUpEligibleAccounts() {
    primary_account_ = identity_test_env()->MakePrimaryAccountAvailable(
        kPrimaryEmail, signin::ConsentLevel::kSignin);
    target_account_ = identity_test_env()->MakeAccountAvailable(kTargetEmail);
    third_account_ = identity_test_env()->MakeAccountAvailable(kThirdEmail);

    const base::Time past_grace_period =
        base::Time::Now() -
        signin::AccountSwitchingManager::kPrimaryAccountSigninGracePeriod;
    SigninPrefs(*GetProfile()->GetPrefs())
        .SetChromeLastSigninTime(primary_account_.GetGaiaId(),
                                 past_grace_period);

    signin::AccountPreviewDataService::AccountPreviewPreference pref;
    pref.gaia_id = target_account_.GetGaiaId();
    pref.preferred_data_types.push_back(
        {syncer::PASSWORDS, signin::SyncDataQuartile::kAboveQ3});
    GetTestAccountPreviewDataService()->SetPreferredAccountForSwitching(pref);
  }

  signin::TestAccountPreviewDataService* GetTestAccountPreviewDataService() {
    return static_cast<signin::TestAccountPreviewDataService*>(
        AccountPreviewDataServiceFactory::GetForProfile(GetProfile()));
  }

  signin::AccountSwitchingManager* GetAccountSwitchingManager() {
    return AccountSwitchingManagerFactory::GetForProfile(GetProfile());
  }

  views::Widget* ShowPromoViaManagerAndWaitForBubble() {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kBubbleWidgetName);
    EXPECT_TRUE(GetAccountSwitchingManager()->ShowPromo(browser()));
    return waiter.WaitIfNeededAndGet();
  }

 protected:
  AccountInfo primary_account_;
  AccountInfo target_account_;
  AccountInfo third_account_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AccountSwitchingBubbleNewProfileArmBrowserTest
    : public AccountSwitchingBubbleBrowserTest {
 public:
  AccountSwitchingBubbleNewProfileArmBrowserTest() {
    arm_feature_list_.InitAndEnableFeatureWithParameters(
        switches::kEnableAccountPreviewSwitchingAccount,
        {{switches::kAccountPreviewSwitchingAccountDesktopArm.name,
          "new_profile"}});
  }

 private:
  base::test::ScopedFeatureList arm_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleBrowserTest,
                       ShowBubbleRendersBothEmailsAndButtons) {
  SetUpEligibleAccounts();

  views::Widget* bubble_widget = ShowPromoViaManagerAndWaitForBubble();
  ASSERT_TRUE(bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());
  EXPECT_TRUE(GetAccountSwitchingManager()->IsPromoActiveForTesting());

  views::WidgetDelegate* widget_delegate = bubble_widget->widget_delegate();
  ASSERT_TRUE(widget_delegate);
  EXPECT_TRUE(widget_delegate->ShouldShowCloseButton());
  EXPECT_EQ(
      widget_delegate->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SWITCHING_IN_PROFILE_BUBBLE_TITLE));

  views::BubbleDialogDelegate* bubble_delegate =
      widget_delegate->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_delegate);
  EXPECT_FALSE(bubble_delegate->ShouldCloseOnDeactivate());
  EXPECT_EQ(bubble_delegate->arrow(), views::BubbleBorder::TOP_RIGHT);

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(bubble_widget);
  auto* primary_email_label = views::AsViewClass<views::Label>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kAccountSwitchingBubblePrimaryAccountEmailElementId, context));
  ASSERT_TRUE(primary_email_label);
  EXPECT_EQ(primary_email_label->GetText(), kPrimaryEmail16);

  auto* target_email_label = views::AsViewClass<views::Label>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kAccountSwitchingBubbleTargetAccountEmailElementId, context));
  ASSERT_TRUE(target_email_label);
  EXPECT_EQ(target_email_label->GetText(), kTargetEmail16);

  auto* accept_button = views::AsViewClass<views::LabelButton>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kAccountSwitchingBubbleAcceptButtonElementId, context));
  ASSERT_TRUE(accept_button);
  EXPECT_EQ(accept_button->GetText(),
            l10n_util::GetStringUTF16(
                IDS_ACCOUNT_SWITCHING_IN_PROFILE_BUBBLE_ACCEPT_BUTTON_LABEL));

  auto* decline_button = views::AsViewClass<views::LabelButton>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kAccountSwitchingBubbleDeclineButtonElementId, context));
  ASSERT_TRUE(decline_button);
  EXPECT_EQ(decline_button->GetText(),
            l10n_util::GetStringUTF16(
                IDS_ACCOUNT_SWITCHING_BUBBLE_CANCEL_BUTTON_LABEL));

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  SigninViewController::From(browser())->CloseBubbleSignin();
  destroyed_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleNewProfileArmBrowserTest,
                       NewProfileArmUsesArmSpecificTitleAndAcceptButtonLabel) {
  SetUpEligibleAccounts();

  views::Widget* bubble_widget = ShowPromoViaManagerAndWaitForBubble();
  ASSERT_TRUE(bubble_widget);

  views::WidgetDelegate* widget_delegate = bubble_widget->widget_delegate();
  ASSERT_TRUE(widget_delegate);
  EXPECT_EQ(widget_delegate->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_ACCOUNT_SWITCHING_NEW_PROFILE_BUBBLE_TITLE));

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(bubble_widget);
  auto* accept_button = views::AsViewClass<views::LabelButton>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kAccountSwitchingBubbleAcceptButtonElementId, context));
  ASSERT_TRUE(accept_button);
  EXPECT_EQ(accept_button->GetText(),
            l10n_util::GetStringUTF16(
                IDS_ACCOUNT_SWITCHING_NEW_PROFILE_BUBBLE_ACCEPT_BUTTON_LABEL));

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  SigninViewController::From(browser())->CloseBubbleSignin();
  destroyed_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleBrowserTest,
                       AcceptButtonStartsSwitchInManager) {
  SetUpEligibleAccounts();

  views::Widget* bubble_widget = ShowPromoViaManagerAndWaitForBubble();
  ASSERT_TRUE(bubble_widget);
  ASSERT_TRUE(GetAccountSwitchingManager()->IsPromoActiveForTesting());

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(bubble_widget);
  auto* accept_button = views::AsViewClass<views::LabelButton>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kAccountSwitchingBubbleAcceptButtonElementId, context));
  ASSERT_TRUE(accept_button);

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  views::test::ButtonTestApi(accept_button).NotifyClick(ui::test::TestEvent());
  destroyed_waiter.Wait();

  EXPECT_FALSE(GetAccountSwitchingManager()->IsPromoActiveForTesting());
  EXPECT_TRUE(GetAccountSwitchingManager()->IsSwitchInProgressForTesting());
}

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleBrowserTest,
                       DeclineButtonRecordsExplicitDismissalInManager) {
  SetUpEligibleAccounts();

  views::Widget* bubble_widget = ShowPromoViaManagerAndWaitForBubble();
  ASSERT_TRUE(bubble_widget);
  ASSERT_TRUE(GetAccountSwitchingManager()->IsPromoActiveForTesting());

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(bubble_widget);
  auto* decline_button = views::AsViewClass<views::LabelButton>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kAccountSwitchingBubbleDeclineButtonElementId, context));
  ASSERT_TRUE(decline_button);

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  views::test::ButtonTestApi(decline_button).NotifyClick(ui::test::TestEvent());
  destroyed_waiter.Wait();

  EXPECT_FALSE(GetAccountSwitchingManager()->IsPromoActiveForTesting());
  EXPECT_FALSE(GetAccountSwitchingManager()->IsSwitchInProgressForTesting());
  EXPECT_EQ(SigninPrefs(*GetProfile()->GetPrefs())
                .GetAccountSwitchingPromoExplicitDismissalCount(
                    primary_account_.GetGaiaId(), target_account_.GetGaiaId()),
            1);
  EXPECT_FALSE(GetAccountSwitchingManager()->CanShowPromo());
}

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleBrowserTest,
                       CloseButtonAndEscReportDismissedWithoutExplicitDecline) {
  SetUpEligibleAccounts();

  for (views::Widget::ClosedReason close_reason :
       {views::Widget::ClosedReason::kCloseButtonClicked,
        views::Widget::ClosedReason::kEscKeyPressed}) {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kBubbleWidgetName);
    base::test::TestFuture<signin::AccountSwitchingPromoResult> result_future;
    base::ScopedClosureRunner close_handle =
        SigninViewController::From(browser())->ShowAccountSwitchingBubble(
            primary_account_, target_account_, result_future.GetCallback());

    views::Widget* bubble_widget = waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(bubble_widget);

    views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
    bubble_widget->CloseWithReason(close_reason);
    destroyed_waiter.Wait();

    EXPECT_EQ(result_future.Get(),
              signin::AccountSwitchingPromoResult::kDismissed);
  }

  // Also verify closing via the `X` button flows back into
  // `AccountSwitchingManager` as an ignore (not an explicit dismissal).
  views::Widget* bubble_widget = ShowPromoViaManagerAndWaitForBubble();
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  bubble_widget->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();

  EXPECT_FALSE(GetAccountSwitchingManager()->IsPromoActiveForTesting());
  EXPECT_FALSE(GetAccountSwitchingManager()->IsSwitchInProgressForTesting());
  EXPECT_EQ(SigninPrefs(*GetProfile()->GetPrefs())
                .GetAccountSwitchingPromoExplicitDismissalCount(
                    primary_account_.GetGaiaId(), target_account_.GetGaiaId()),
            0);
  EXPECT_FALSE(GetAccountSwitchingManager()->CanShowPromo());
}

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleBrowserTest,
                       ClosesOnAccountInvalidationAndIgnoresUnrelatedChanges) {
  SetUpEligibleAccounts();

  auto show_and_verify_closes_ignored = [&](auto trigger) {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kBubbleWidgetName);
    base::test::TestFuture<signin::AccountSwitchingPromoResult> result_future;
    base::ScopedClosureRunner close_handle =
        SigninViewController::From(browser())->ShowAccountSwitchingBubble(
            primary_account_, target_account_, result_future.GetCallback());
    views::Widget* bubble_widget = waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(bubble_widget);

    views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
    trigger();
    destroyed_waiter.Wait();
    EXPECT_EQ(result_future.Get(),
              signin::AccountSwitchingPromoResult::kIgnored);
  };

  // Unrelated third account token removal and transient errors on the target
  // account must not close the bubble.
  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kBubbleWidgetName);
    base::test::TestFuture<signin::AccountSwitchingPromoResult> result_future;
    base::ScopedClosureRunner close_handle =
        SigninViewController::From(browser())->ShowAccountSwitchingBubble(
            primary_account_, target_account_, result_future.GetCallback());
    views::Widget* bubble_widget = waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(bubble_widget);

    identity_test_env()->RemoveRefreshTokenForAccount(
        third_account_.GetAccountId());
    EXPECT_TRUE(bubble_widget->IsVisible());
    EXPECT_FALSE(result_future.IsReady());

    identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
        target_account_.GetAccountId(),
        GoogleServiceAuthError::FromServiceUnavailable("transient"));
    EXPECT_TRUE(bubble_widget->IsVisible());
    EXPECT_FALSE(result_future.IsReady());

    views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
    close_handle.RunAndReset();
    destroyed_waiter.Wait();
    EXPECT_EQ(result_future.Get(),
              signin::AccountSwitchingPromoResult::kIgnored);
  }
  third_account_ = identity_test_env()->MakeAccountAvailable(kThirdEmail);
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      target_account_.GetAccountId(), GoogleServiceAuthError::AuthErrorNone());

  // Primary account persistent auth error closes the bubble.
  show_and_verify_closes_ignored([&]() {
    identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
        primary_account_.GetAccountId(),
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
  });
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_.GetAccountId(), GoogleServiceAuthError::AuthErrorNone());

  // Target account persistent auth error closes the bubble.
  show_and_verify_closes_ignored([&]() {
    identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
        target_account_.GetAccountId(),
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
  });
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      target_account_.GetAccountId(), GoogleServiceAuthError::AuthErrorNone());

  // Primary account changed directly (`Type::kSet`) closes the bubble.
  show_and_verify_closes_ignored([&]() {
    auto* mutator = identity_manager()->GetPrimaryAccountMutator();
    ASSERT_TRUE(mutator);
    ASSERT_EQ(mutator->SetPrimaryAccount(
                  third_account_.GetAccountId(), signin::ConsentLevel::kSignin,
                  signin_metrics::AccessPoint::kStartPage),
              signin::PrimaryAccountMutator::PrimaryAccountError::kNoError);
  });
  ASSERT_EQ(identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
                primary_account_.GetAccountId(), signin::ConsentLevel::kSignin,
                signin_metrics::AccessPoint::kStartPage),
            signin::PrimaryAccountMutator::PrimaryAccountError::kNoError);
  SigninPrefs(*GetProfile()->GetPrefs())
      .SetChromeLastSigninTime(primary_account_.GetGaiaId(),
                               base::Time::Now() -
                                   signin::AccountSwitchingManager::
                                       kPrimaryAccountSigninGracePeriod);

  // Target refresh token removed closes the bubble.
  show_and_verify_closes_ignored([&]() {
    identity_test_env()->RemoveRefreshTokenForAccount(
        target_account_.GetAccountId());
  });
  target_account_ = identity_test_env()->MakeAccountAvailable(kTargetEmail);

  // Primary account cleared while shown via AccountSwitchingManager closes the
  // bubble and clears the active promo state.
  views::Widget* bubble_widget = ShowPromoViaManagerAndWaitForBubble();
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  identity_test_env()->ClearPrimaryAccount();
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetAccountSwitchingManager()->IsPromoActiveForTesting());

  // Primary refresh token removed closes the bubble.
  primary_account_ = identity_test_env()->MakePrimaryAccountAvailable(
      kPrimaryEmail, signin::ConsentLevel::kSignin);
  target_account_ = identity_test_env()->MakeAccountAvailable(kTargetEmail);
  show_and_verify_closes_ignored([&]() {
    identity_test_env()->RemoveRefreshTokenForAccount(
        primary_account_.GetAccountId());
  });
}

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleBrowserTest,
                       ClosingBrowserWindowReportsIgnoredToManager) {
  SetUpEligibleAccounts();

  BrowserWindowInterface* other_browser = CreateBrowser(GetProfile());
  ASSERT_TRUE(other_browser);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kBubbleWidgetName);
  ASSERT_TRUE(GetAccountSwitchingManager()->ShowPromo(other_browser));
  ASSERT_TRUE(waiter.WaitIfNeededAndGet());
  ASSERT_TRUE(GetAccountSwitchingManager()->IsPromoActiveForTesting());

  CloseBrowserSynchronously(other_browser);

  EXPECT_FALSE(GetAccountSwitchingManager()->IsPromoActiveForTesting());
  EXPECT_FALSE(GetAccountSwitchingManager()->IsSwitchInProgressForTesting());
  EXPECT_FALSE(GetAccountSwitchingManager()->CanShowPromo());
}

IN_PROC_BROWSER_TEST_F(AccountSwitchingBubbleBrowserTest,
                       ManagerShutdownClosesBubbleWithoutThrottling) {
  SetUpEligibleAccounts();

  views::Widget* bubble_widget = ShowPromoViaManagerAndWaitForBubble();
  ASSERT_TRUE(bubble_widget);
  ASSERT_TRUE(GetAccountSwitchingManager()->IsPromoActiveForTesting());

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  GetAccountSwitchingManager()->Shutdown();
  destroyed_waiter.Wait();

  EXPECT_FALSE(GetAccountSwitchingManager()->IsPromoActiveForTesting());
  EXPECT_TRUE(GetAccountSwitchingManager()->CanShowPromo());
}

}  // namespace
