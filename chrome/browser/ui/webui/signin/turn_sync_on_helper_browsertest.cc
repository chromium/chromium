// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace {

class Delegate : public TurnSyncOnHelper::Delegate {
 public:
  enum class BlockingStep {
    kNone,
    kMergeData,
    kEnterpriseManagement,
    kSyncConfirmation,
    kSyncDisabled,
  };

  struct Choices {
    std::optional<signin::SigninChoice> merge_data_choice =
        signin::SIGNIN_CHOICE_CONTINUE;
    std::optional<signin::SigninChoice> enterprise_management_choice =
        signin::SIGNIN_CHOICE_CONTINUE;
    std::optional<LoginUIService::SyncConfirmationUIClosedResult>
        sync_optin_choice = LoginUIService::SYNC_WITH_DEFAULT_SETTINGS;
    std::optional<LoginUIService::SyncConfirmationUIClosedResult>
        sync_disabled_choice = std::nullopt;
  };

  using SyncConfirmationCallback =
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>;
  using CallbackVariant =
      absl::variant<signin::SigninChoiceCallback, SyncConfirmationCallback>;

  explicit Delegate(Choices choices)
      : choices_(choices), run_loop_(std::make_unique<base::RunLoop>()) {}
  ~Delegate() override = default;

  // TurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override {
    NOTREACHED_IN_MIGRATION();
  }
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override {
    AdvanceFlowOrCapture(BlockingStep::kMergeData, std::move(callback));
  }
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) override {
    AdvanceFlowOrCapture(BlockingStep::kEnterpriseManagement,
                         std::move(callback));
  }
  void ShowSyncConfirmation(SyncConfirmationCallback callback) override {
    AdvanceFlowOrCapture(BlockingStep::kSyncConfirmation, std::move(callback));
  }
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      SyncConfirmationCallback callback) override {
    AdvanceFlowOrCapture(BlockingStep::kSyncDisabled, std::move(callback));
  }
  void ShowSyncSettings() override { NOTREACHED_IN_MIGRATION(); }
  void SwitchToProfile(Profile* new_profile) override {
    NOTREACHED_IN_MIGRATION();
  }

  BlockingStep blocking_step() const { return blocking_step_; }

  base::WeakPtr<Delegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void WaitUntilBlock() {
    DCHECK(run_loop_);
    run_loop_->Run();

    // After the wait ends, reset the `run_loop_` to wait for the next choice.
    run_loop_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  // Call this function when the delegate is blocked, with new `choices` that
  // should now unblock it.
  void UpdateChoicesAndAdvanceFlow(Choices choices) {
    auto blocking_step = blocking_step_;
    ASSERT_NE(BlockingStep::kNone, blocking_step)
        << "UpdateChoicesAndAdvanceFlow() expects to be called while the "
           "delegate is blocked.";
    choices_ = choices;
    blocking_step_ = BlockingStep::kNone;

    switch (blocking_step) {
      case BlockingStep::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
      case BlockingStep::kMergeData:
        ASSERT_TRUE(choices_.merge_data_choice.has_value());
        std::move(absl::get<signin::SigninChoiceCallback>(blocking_callback_))
            .Run(*choices_.merge_data_choice);
        break;
      case BlockingStep::kEnterpriseManagement:
        ASSERT_TRUE(choices_.enterprise_management_choice.has_value());
        std::move(absl::get<signin::SigninChoiceCallback>(blocking_callback_))
            .Run(*choices_.enterprise_management_choice);
        break;
      case BlockingStep::kSyncConfirmation:
        ASSERT_TRUE(choices_.sync_optin_choice.has_value());
        std::move(absl::get<SyncConfirmationCallback>(blocking_callback_))
            .Run(*choices_.sync_optin_choice);
        break;
      case BlockingStep::kSyncDisabled:
        ASSERT_TRUE(choices_.sync_disabled_choice.has_value());
        std::move(absl::get<SyncConfirmationCallback>(blocking_callback_))
            .Run(*choices_.sync_disabled_choice);
        break;
    }
  }

 private:
  void AdvanceFlowOrCapture(BlockingStep step, CallbackVariant callback) {
    switch (step) {
      case BlockingStep::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
      case BlockingStep::kMergeData:
        if (!choices_.merge_data_choice.has_value()) {
          break;
        }
        std::move(absl::get<signin::SigninChoiceCallback>(callback))
            .Run(*choices_.merge_data_choice);
        return;
      case BlockingStep::kEnterpriseManagement:
        if (!choices_.enterprise_management_choice.has_value()) {
          break;
        }
        std::move(absl::get<signin::SigninChoiceCallback>(callback))
            .Run(*choices_.enterprise_management_choice);
        return;
      case BlockingStep::kSyncConfirmation:
        if (!choices_.sync_optin_choice.has_value()) {
          break;
        }
        std::move(absl::get<SyncConfirmationCallback>(callback))
            .Run(*choices_.sync_optin_choice);
        return;
      case BlockingStep::kSyncDisabled:
        if (!choices_.sync_disabled_choice.has_value()) {
          break;
        }
        std::move(absl::get<SyncConfirmationCallback>(callback))
            .Run(*choices_.sync_disabled_choice);
        return;
    }

    blocking_step_ = step;
    blocking_callback_ = std::move(callback);
    DCHECK(run_loop_);
    run_loop_->Quit();
  }

  Choices choices_;

  BlockingStep blocking_step_ = BlockingStep::kNone;
  CallbackVariant blocking_callback_;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
};

}  // namespace

// Test params:
// - TurnSyncOnHelper::SigninAbortedMode: abort mode.
// - bool: should_remove_initial_account
// - bool: Explicit browser signin feature
class TurnSyncOnHelperBrowserTestWithParam
    : public SigninBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<TurnSyncOnHelper::SigninAbortedMode, bool, bool>> {
 public:
  TurnSyncOnHelperBrowserTestWithParam()
      : SigninBrowserTestBase(/*use_main_profile=*/false) {
    scoped_feature_list_.InitWithFeatureState(
        switches::kExplicitBrowserSigninUIOnDesktop,
        is_explicit_browser_signin_enabled());
  }

 protected:
  bool should_remove_initial_account() const { return std::get<1>(GetParam()); }

  TurnSyncOnHelper::SigninAbortedMode aborted_mode() const {
    return std::get<TurnSyncOnHelper::SigninAbortedMode>(GetParam());
  }

  bool is_explicit_browser_signin_enabled() const {
    return std::get<2>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that aborting a Sync opt-in flow started with a secondary account
// reverts the primary account to the initial one.
IN_PROC_BROWSER_TEST_P(TurnSyncOnHelperBrowserTestWithParam,
                       PrimaryAccountResetAfterSyncOptInFlowAborted) {
  Profile* profile = GetProfile();
  CoreAccountInfo primary_account_info = signin::MakeAccountAvailable(
      identity_manager(), identity_test_env()
                              ->CreateAccountAvailabilityOptionsBuilder()
                              .AsPrimary(signin::ConsentLevel::kSignin)
                              .WithCookie()
                              .Build("first@gmail.com"));
  auto secondary_accounts_info =
      SetAccountsCookiesAndTokens({"second@gmail.com", "third@gmail.com"});
  AccountInfo second_account_info = secondary_accounts_info[0];
  AccountInfo third_account_info = secondary_accounts_info[1];
  CoreAccountId first_account_id = primary_account_info.account_id;
  CoreAccountId second_account_id = second_account_info.account_id;

  ASSERT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  ASSERT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                  signin::ConsentLevel::kSignin));

  base::RunLoop run_loop;
  Delegate::Choices choices = {.sync_optin_choice = std::nullopt};
  auto owned_delegate = std::make_unique<Delegate>(choices);
  base::WeakPtr<Delegate> delegate = owned_delegate->GetWeakPtr();
  new TurnSyncOnHelper(
      profile, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      second_account_id, aborted_mode(), std::move(owned_delegate),
      run_loop.QuitClosure());

  delegate->WaitUntilBlock();
  EXPECT_EQ(Delegate::BlockingStep::kSyncConfirmation,
            delegate->blocking_step());
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(second_account_id, identity_manager()->GetPrimaryAccountId(
                                   signin::ConsentLevel::kSignin));

  if (should_remove_initial_account()) {
    identity_manager()->GetAccountsMutator()->RemoveAccount(
        first_account_id,
        signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  }

  choices.sync_optin_choice = LoginUIService::ABORT_SYNC;
  delegate->UpdateChoicesAndAdvanceFlow(choices);

  // The flow should complete and destroy the delegate and TurnSyncOnHelper.
  run_loop.Run();
  EXPECT_FALSE(delegate);

  // Check expectations.
  switch (aborted_mode()) {
    case TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT:
      if (should_remove_initial_account()) {
        // All accounts are removed.
        EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
        EXPECT_FALSE(identity_manager()->HasPrimaryAccount(
            signin::ConsentLevel::kSignin));
      } else {
        // Second account removed, first account is still primary.
        EXPECT_THAT(
            identity_manager()->GetAccountsWithRefreshTokens(),
            UnorderedElementsAre(primary_account_info, third_account_info));
        EXPECT_EQ(signin::ConsentLevel::kSignin,
                  signin::GetPrimaryAccountConsentLevel(identity_manager()));
        EXPECT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                        signin::ConsentLevel::kSignin));
      }
      break;
    case TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT:
      if (should_remove_initial_account()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        // First account was removed, second account became the primary.
        EXPECT_THAT(
            identity_manager()->GetAccountsWithRefreshTokens(),
            UnorderedElementsAre(second_account_info, third_account_info));
        EXPECT_EQ(signin::ConsentLevel::kSignin,
                  signin::GetPrimaryAccountConsentLevel(identity_manager()));
        EXPECT_EQ(second_account_id, identity_manager()->GetPrimaryAccountId(
                                         signin::ConsentLevel::kSignin));
#else
        // With `switches::kExplicitBrowserSigninUIOnDesktop` enabled, the
        // primary account isn't set implicitly based on cookies but by explicit
        // user action, therefore it is also not removed when cookies change.
        // The account should remain and Chrome still signed in.
        if (is_explicit_browser_signin_enabled()) {
          EXPECT_FALSE(
              identity_manager()->GetAccountsWithRefreshTokens().empty());
          EXPECT_TRUE(identity_manager()->HasPrimaryAccount(
              signin::ConsentLevel::kSignin));
        } else {
          EXPECT_TRUE(
              identity_manager()->GetAccountsWithRefreshTokens().empty());
          EXPECT_FALSE(identity_manager()->HasPrimaryAccount(
              signin::ConsentLevel::kSignin));
        }
#endif
      } else {
        // First account is still primary, second account was not removed.
        EXPECT_THAT(
            identity_manager()->GetAccountsWithRefreshTokens(),
            UnorderedElementsAre(primary_account_info, second_account_info,
                                 third_account_info));
        EXPECT_EQ(signin::ConsentLevel::kSignin,
                  signin::GetPrimaryAccountConsentLevel(identity_manager()));
        EXPECT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                        signin::ConsentLevel::kSignin));
      }
      break;
    case TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY:
      // This case is handled in the TurnSyncOnHelperBrowserTestWithUnoDesktop
      // test suite, since this mode is used only when Uno Desktop is enabled.
      NOTREACHED_IN_MIGRATION();
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TurnSyncOnHelperBrowserTestWithParam,
    testing::Combine(
        testing::Values(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT,
                        TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT),
        // Whether the initial account should be removed during the flow.
        testing::Bool(),
        testing::Bool()));

class TurnSyncOnHelperBrowserTest : public SigninBrowserTestBase {
 public:
  TurnSyncOnHelperBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/false) {}
};

// Regression test for https://crbug.com/1404961
IN_PROC_BROWSER_TEST_F(TurnSyncOnHelperBrowserTest, UndoSyncRemoveAccount) {
  Profile* profile = GetProfile();

  CoreAccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager(), identity_test_env()
                              ->CreateAccountAvailabilityOptionsBuilder()
                              .AsPrimary(signin::ConsentLevel::kSignin)
                              .WithCookie()
                              .Build("account@gmail.com"));
  CoreAccountId account_id = account_info.account_id;

  base::RunLoop run_loop;
  Delegate::Choices choices = {.sync_optin_choice = std::nullopt};
  auto owned_delegate = std::make_unique<Delegate>(choices);
  base::WeakPtr<Delegate> delegate = owned_delegate->GetWeakPtr();
  new TurnSyncOnHelper(
      profile, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO, account_id,
      TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT,
      std::move(owned_delegate), run_loop.QuitClosure());

  delegate->WaitUntilBlock();
  EXPECT_EQ(Delegate::BlockingStep::kSyncConfirmation,
            delegate->blocking_step());
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(account_id, identity_manager()->GetPrimaryAccountId(
                            signin::ConsentLevel::kSignin));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  // For the scenario in https://crbug.com/1404961, the reconcilor has to be
  // triggered by the account removal.
  ASSERT_EQ(reconcilor->GetState(),
            signin_metrics::AccountReconcilorState::kOk);

  choices.sync_optin_choice = LoginUIService::ABORT_SYNC;
  delegate->UpdateChoicesAndAdvanceFlow(choices);

  // The flow should complete and destroy the delegate and TurnSyncOnHelper.
  run_loop.Run();
  EXPECT_FALSE(delegate);
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // For the scenario in https://crbug.com/1404961, the reconcilor has to be
  // triggered by the account removal.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ASSERT_EQ(reconcilor->GetState(),
            signin_metrics::AccountReconcilorState::kRunning);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Dice platforms with `switches::kExplicitBrowserSigninUIOnDesktop`
  // enabled and empty primary account, updating cookies is disabled. Therefore
  // running the reconcilor doesn't require any network requests and might have
  // been completed by now. The reconcilor will not remove the account from
  // cookies but revoking refresh tokens should be sufficient to invalidate
  // cookies.
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class TurnSyncOnHelperBrowserTestWithUnoDesktop
    : public TurnSyncOnHelperBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

// Tests that aborting a Sync opt-in flow started with a web only signed in
// account reverts the account to the initial web only signed in state.
IN_PROC_BROWSER_TEST_F(TurnSyncOnHelperBrowserTestWithUnoDesktop,
                       WebOnlyAccountResetAfterSyncOptInFlowAborted) {
  Profile* profile = GetProfile();
  // Set up first account.
  AccountInfo first_account_info =
      identity_test_env()->MakeAccountAvailable("first@gmail.com");
  identity_test_env()->UpdateAccountInfoForAccount(first_account_info);
  CoreAccountId first_account_id = first_account_info.account_id;

  ASSERT_NE(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::RunLoop run_loop;
  Delegate::Choices choices = {.sync_optin_choice = std::nullopt};
  auto owned_delegate = std::make_unique<Delegate>(choices);
  base::WeakPtr<Delegate> delegate = owned_delegate->GetWeakPtr();
  new TurnSyncOnHelper(
      profile, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      first_account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY,
      std::move(owned_delegate), run_loop.QuitClosure());

  delegate->WaitUntilBlock();
  EXPECT_EQ(Delegate::BlockingStep::kSyncConfirmation,
            delegate->blocking_step());
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                  signin::ConsentLevel::kSignin));

  choices.sync_optin_choice = LoginUIService::ABORT_SYNC;
  delegate->UpdateChoicesAndAdvanceFlow(choices);

  // The flow should complete and destroy the delegate and TurnSyncOnHelper.
  run_loop.Run();
  EXPECT_FALSE(delegate);

  // Check expectations.
  EXPECT_THAT(identity_manager()->GetAccountsWithRefreshTokens(),
              UnorderedElementsAre(first_account_info));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

// Tests that aborting a Sync opt-in flow started with a secondary account
// reverts the primary account to the initial one.
IN_PROC_BROWSER_TEST_F(
    TurnSyncOnHelperBrowserTestWithUnoDesktop,
    PrimaryAccountResetAfterSyncOptInFlowAbortedForSecondaryAccount) {
  Profile* profile = GetProfile();
  // Set up the primary account.
  CoreAccountInfo primary_account_info = signin::MakeAccountAvailable(
      identity_manager(), identity_test_env()
                              ->CreateAccountAvailabilityOptionsBuilder()
                              .AsPrimary(signin::ConsentLevel::kSignin)
                              .WithCookie()
                              .Build("first@gmail.com"));
  auto secondary_accounts_info =
      SetAccountsCookiesAndTokens({"second@gmail.com", "third@gmail.com"});
  AccountInfo second_account_info = secondary_accounts_info[0];
  AccountInfo third_account_info = secondary_accounts_info[1];
  CoreAccountId first_account_id = primary_account_info.account_id;
  CoreAccountId second_account_id = second_account_info.account_id;

  ASSERT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  ASSERT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                  signin::ConsentLevel::kSignin));

  base::RunLoop run_loop;
  Delegate::Choices choices = {.sync_optin_choice = std::nullopt};
  auto owned_delegate = std::make_unique<Delegate>(choices);
  base::WeakPtr<Delegate> delegate = owned_delegate->GetWeakPtr();
  new TurnSyncOnHelper(
      profile, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      second_account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY,
      std::move(owned_delegate), run_loop.QuitClosure());

  delegate->WaitUntilBlock();
  EXPECT_EQ(Delegate::BlockingStep::kSyncConfirmation,
            delegate->blocking_step());
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(second_account_id, identity_manager()->GetPrimaryAccountId(
                                   signin::ConsentLevel::kSignin));

  choices.sync_optin_choice = LoginUIService::ABORT_SYNC;
  delegate->UpdateChoicesAndAdvanceFlow(choices);

  // The flow should complete and destroy the delegate and TurnSyncOnHelper.
  run_loop.Run();
  EXPECT_FALSE(delegate);

  // First account is still primary, second account was not removed.
  EXPECT_THAT(identity_manager()->GetAccountsWithRefreshTokens(),
              UnorderedElementsAre(primary_account_info, second_account_info,
                                   third_account_info));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                  signin::ConsentLevel::kSignin));
}

// Tests that aborting a Sync opt-in flow started with a new secondary account
// reverts the primary account to the initial one and removes the new account.
IN_PROC_BROWSER_TEST_F(
    TurnSyncOnHelperBrowserTestWithUnoDesktop,
    PrimaryAccountResetAfterSyncOptInFlowAbortedForNewAccount) {
  Profile* profile = GetProfile();

  // Set up the primary account.
  CoreAccountInfo primary_account_info = signin::MakeAccountAvailable(
      identity_manager(), identity_test_env()
                              ->CreateAccountAvailabilityOptionsBuilder()
                              .AsPrimary(signin::ConsentLevel::kSignin)
                              .WithCookie()
                              .Build("first@gmail.com"));
  CoreAccountId first_account_id = primary_account_info.account_id;
  auto secondary_accounts_info =
      SetAccountsCookiesAndTokens({"second@gmail.com"});
  AccountInfo second_account_info = secondary_accounts_info[0];
  CoreAccountId second_account_id = second_account_info.account_id;

  ASSERT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  ASSERT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                  signin::ConsentLevel::kSignin));

  base::RunLoop run_loop;
  Delegate::Choices choices = {.sync_optin_choice = std::nullopt};
  auto owned_delegate = std::make_unique<Delegate>(choices);
  base::WeakPtr<Delegate> delegate = owned_delegate->GetWeakPtr();
  new TurnSyncOnHelper(
      profile, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      second_account_id, TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT,
      std::move(owned_delegate), run_loop.QuitClosure());

  delegate->WaitUntilBlock();
  EXPECT_EQ(Delegate::BlockingStep::kSyncConfirmation,
            delegate->blocking_step());
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(second_account_id, identity_manager()->GetPrimaryAccountId(
                                   signin::ConsentLevel::kSignin));

  choices.sync_optin_choice = LoginUIService::ABORT_SYNC;
  delegate->UpdateChoicesAndAdvanceFlow(choices);

  // The flow should complete and destroy the delegate and TurnSyncOnHelper.
  run_loop.Run();
  EXPECT_FALSE(delegate);

  // First account is still primary, second account was removed.
  EXPECT_THAT(identity_manager()->GetAccountsWithRefreshTokens(),
              UnorderedElementsAre(primary_account_info));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(first_account_id, identity_manager()->GetPrimaryAccountId(
                                  signin::ConsentLevel::kSignin));
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
