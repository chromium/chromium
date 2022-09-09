// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    absl::optional<signin::SigninChoice> merge_data_choice =
        signin::SIGNIN_CHOICE_CONTINUE;
    absl::optional<signin::SigninChoice> enterprise_management_choice =
        signin::SIGNIN_CHOICE_CONTINUE;
    absl::optional<LoginUIService::SyncConfirmationUIClosedResult>
        sync_optin_choice = LoginUIService::SYNC_WITH_DEFAULT_SETTINGS;
    absl::optional<LoginUIService::SyncConfirmationUIClosedResult>
        sync_disabled_choice = absl::nullopt;
  };

  using SyncConfirmationCallback =
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>;
  using CallbackVariant =
      absl::variant<signin::SigninChoiceCallback, SyncConfirmationCallback>;

  explicit Delegate(Choices choices)
      : choices_(choices), run_loop_(std::make_unique<base::RunLoop>()) {}
  ~Delegate() override = default;

  // TurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override { NOTREACHED(); }
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
  void ShowSyncSettings() override { NOTREACHED(); }
  void SwitchToProfile(Profile* new_profile) override { NOTREACHED(); }

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
        NOTREACHED();
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
        NOTREACHED();
        break;
      case BlockingStep::kMergeData:
        if (!choices_.merge_data_choice.has_value())
          break;
        std::move(absl::get<signin::SigninChoiceCallback>(callback))
            .Run(*choices_.merge_data_choice);
        return;
      case BlockingStep::kEnterpriseManagement:
        if (!choices_.enterprise_management_choice.has_value())
          break;
        std::move(absl::get<signin::SigninChoiceCallback>(callback))
            .Run(*choices_.enterprise_management_choice);
        return;
      case BlockingStep::kSyncConfirmation:
        if (!choices_.sync_optin_choice.has_value())
          break;
        std::move(absl::get<SyncConfirmationCallback>(callback))
            .Run(*choices_.sync_optin_choice);
        return;
      case BlockingStep::kSyncDisabled:
        if (!choices_.sync_disabled_choice.has_value())
          break;
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

enum class SyncTiming { kEager, kDelayed };

}  // namespace

class TurnSyncOnHelperBrowserTest
    : public SigninBrowserTestBase,
      public ::testing::WithParamInterface<SyncTiming> {
 public:
  TurnSyncOnHelperBrowserTest() {
    if (GetParam() == SyncTiming::kEager) {
      scoped_feature_list_.InitAndDisableFeature(kDelayConsentLevelUpgrade);
    } else {
      scoped_feature_list_.InitAndEnableFeature(kDelayConsentLevelUpgrade);
    }
  }

  signin::ConsentLevel GetExpectedConsentLevelBeforeSyncConfirm() const {
    return GetParam() == SyncTiming::kEager ? signin::ConsentLevel::kSync
                                            : signin::ConsentLevel::kSignin;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(https://crbug.com/1344165): Support resetting the primary account.
#define MAYBE_PrimaryAccountResetAfterSyncOptInFlowAborted \
  DISABLED_PrimaryAccountResetAfterSyncOptInFlowAborted
#else
#define MAYBE_PrimaryAccountResetAfterSyncOptInFlowAborted \
  PrimaryAccountResetAfterSyncOptInFlowAborted
#endif
// Tests that aborting a Sync opt-in flow started with a secondary account
// reverts the primary account to the initial one.
IN_PROC_BROWSER_TEST_P(TurnSyncOnHelperBrowserTest,
                       MAYBE_PrimaryAccountResetAfterSyncOptInFlowAborted) {
  Profile* profile = browser()->profile();
  auto accounts_info =
      SetAccounts({"primary@gmail.com", "secondary@gmail.com"});
  auto primary_account_id = accounts_info[0].account_id;
  auto secondary_account_id = accounts_info[1].account_id;

  ASSERT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  ASSERT_EQ(primary_account_id, identity_manager()->GetPrimaryAccountId(
                                    signin::ConsentLevel::kSignin));

  base::RunLoop run_loop;
  Delegate::Choices choices = {.sync_optin_choice = absl::nullopt};
  auto owned_delegate = std::make_unique<Delegate>(choices);
  base::WeakPtr<Delegate> delegate = owned_delegate->GetWeakPtr();
  new TurnSyncOnHelper(
      profile, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::kUnknownReason, secondary_account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::move(owned_delegate), run_loop.QuitClosure());

  delegate->WaitUntilBlock();
  EXPECT_EQ(Delegate::BlockingStep::kSyncConfirmation,
            delegate->blocking_step());
  EXPECT_EQ(GetExpectedConsentLevelBeforeSyncConfirm(),
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(secondary_account_id, identity_manager()->GetPrimaryAccountId(
                                      signin::ConsentLevel::kSignin));

  choices.sync_optin_choice = LoginUIService::ABORT_SYNC;
  delegate->UpdateChoicesAndAdvanceFlow(choices);

  // The flow should complete and destroy the delegate and TurnSyncOnHelper.
  run_loop.Run();
  EXPECT_FALSE(delegate);

  // Account consistency (via `SigninManager`) should kick in via a posted task
  // and reset the PrimaryAccount.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_EQ(primary_account_id, identity_manager()->GetPrimaryAccountId(
                                    signin::ConsentLevel::kSignin));
}

INSTANTIATE_TEST_SUITE_P(,
                         TurnSyncOnHelperBrowserTest,
                         ::testing::Values(SyncTiming::kEager,
                                           SyncTiming::kDelayed),
                         [](const ::testing::TestParamInfo<SyncTiming>& info) {
                           return info.param == SyncTiming::kEager ? "Eager"
                                                                   : "Delayed";
                         });
