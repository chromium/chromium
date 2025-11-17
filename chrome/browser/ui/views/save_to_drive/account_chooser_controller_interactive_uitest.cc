// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_controller.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_test_util.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace save_to_drive {
namespace {

using ::save_to_drive::testing::GetTestAccount;
using ::save_to_drive::testing::GetTestAccounts;

constexpr char kAvatarUrl[] = "https://avatar.com/avatar.png";

AccountChosenCallback GetOnAccountChosenCallback(
    const AccountInfo& expected_account,
    content::WaiterHelper& waiter) {
  return base::BindLambdaForTesting(
      [&expected_account, &waiter](std::optional<AccountInfo> account) {
        // Cannot match by id because IdentityTestEnvironment generates an
        // id on your behalf.
        EXPECT_EQ(account->full_name, expected_account.full_name);
        EXPECT_EQ(account->email, expected_account.email);
        waiter.OnEvent();
      });
}

AccountChosenCallback GetOnNoAccountChosenCallback(
    content::WaiterHelper& waiter) {
  return base::BindLambdaForTesting(
      [&waiter](std::optional<AccountInfo> account) {
        EXPECT_FALSE(account.has_value());
        waiter.OnEvent();
      });
}

class AccountChooserControllerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  AccountChooserControllerInteractiveUiTest() = default;
  ~AccountChooserControllerInteractiveUiTest() override = default;

  // InteractiveBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&AccountChooserControllerInteractiveUiTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  // InteractiveBrowserTest:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  // InteractiveBrowserTest:
  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    // Destruction order matters.
    identity_test_environment_adaptor_.reset();
    account_chooser_controller_.reset();
  }

  auto CreateAccountChooserController() {
    return Do([this]() {
      account_chooser_controller_ = std::make_unique<AccountChooserController>(
          browser()->tab_strip_model()->GetActiveWebContents(),
          identity_test_environment_adaptor_->identity_test_env()
              ->identity_manager());
    });
  }

  auto MakeAccountAvailable(const AccountInfo& account) {
    return Do(
        [this, &account]() { MakeAccountAvailableInIdentityTestEnv(account); });
  }

  // Stores the persisted account in the out parameter `persisted_account`.
  auto MakeAccountAvailable(const AccountInfo& account,
                            AccountInfo* persisted_account) {
    return Do([this, &account, persisted_account]() {
      *persisted_account = MakeAccountAvailableInIdentityTestEnv(account);
    });
  }

  // Stores the persisted accounts in the out parameter `persisted_accounts`.
  auto MakeAccountsAvailable(const std::vector<AccountInfo>& accounts,
                             std::vector<AccountInfo>* persisted_accounts) {
    return Do([this, &accounts, persisted_accounts]() {
      persisted_accounts->clear();
      for (const auto& account : accounts) {
        AccountInfo persisted_account =
            MakeAccountAvailableInIdentityTestEnv(account);
        persisted_accounts->push_back(std::move(persisted_account));
      }
    });
  }

  AccountInfo MakeAccountAvailableInIdentityTestEnv(
      const AccountInfo& account) {
    signin::IdentityTestEnvironment* identity_test_env =
        identity_test_environment_adaptor_->identity_test_env();
    AccountInfo persisted_account =
        identity_test_env->MakeAccountAvailable(account.email);
    persisted_account.full_name = account.full_name;
    persisted_account.account_image = account.account_image;
    identity_test_env->UpdateAccountInfoForAccount(persisted_account);
    signin::SimulateAccountImageFetch(identity_test_env->identity_manager(),
                                      persisted_account.account_id, kAvatarUrl,
                                      persisted_account.account_image);
    return persisted_account;
  }

  auto GetAccount(AccountChosenCallback on_account_chosen_callback) {
    return Do([this, on_account_chosen_callback =
                         std::move(on_account_chosen_callback)]() mutable {
      account_chooser_controller_->GetAccount(
          std::move(on_account_chosen_callback));
    });
  }

  auto VerifyPopupOpened() {
    return Steps(
        CheckResult([]() -> size_t { return chrome::GetTotalBrowserCount(); },
                    2u, "Expect two browsers."),
        Check(
            []() {
              return ui_test_utils::FindMatchingBrowsers(
                         [](BrowserWindowInterface* browser) {
                           return browser->GetType() ==
                                  BrowserWindowInterface::Type::TYPE_POPUP;
                         })
                         .size() == 1;
            },
            "Expect second browser is popup."));
  }

  // Must be called after VerifyPopupOpened().
  auto ClosePopup() {
    return Do([]() {
      BrowserWindowInterface* const popup_browser =
          ui_test_utils::FindMatchingBrowsers(
              [](BrowserWindowInterface* browser) {
                return browser->GetType() ==
                       BrowserWindowInterface::Type::TYPE_POPUP;
              })
              .front();
      CHECK(popup_browser);
      popup_browser->GetWindow()->Close();
    });
  }

  auto VerifyPopupClosed() {
    return CheckResult(
        []() -> size_t { return chrome::GetTotalBrowserCount(); }, 1u,
        "Expect one browser.");
  }

  auto MakeAccountAvailableAndSimulateOnExtendedAccountInfoUpdated(
      const AccountInfo& account) {
    return Do([this, &account]() {
      AccountInfo persisted_account =
          MakeAccountAvailableInIdentityTestEnv(account);
    });
  }

  auto
  RemoveAccountFromIdentityTestEnvAndSimulateOnRefreshTokenRemovedForAccount(
      AccountInfo* account) {
    return Do([this, account]() {
      signin::IdentityTestEnvironment* identity_test_env =
          identity_test_environment_adaptor_->identity_test_env();
      identity_test_env->RemoveRefreshTokenForAccount(account->account_id);
    });
  }

  auto MakePrimaryAccountAvailable(const AccountInfo& account,
                                   AccountInfo* persisted_account) {
    return Do([this, &account, persisted_account]() {
      signin::IdentityTestEnvironment* identity_test_env =
          identity_test_environment_adaptor_->identity_test_env();
      *persisted_account = identity_test_env->MakePrimaryAccountAvailable(
          account.email, signin::ConsentLevel::kSignin);
      persisted_account->full_name = account.full_name;
      persisted_account->account_image = account.account_image;
      identity_test_env->UpdateAccountInfoForAccount(*persisted_account);
      signin::SimulateAccountImageFetch(
          identity_test_env->identity_manager(), persisted_account->account_id,
          kAvatarUrl, persisted_account->account_image);
    });
  }

  auto MakePrimaryAccountAvailableWithInvalidRefreshToken(
      const AccountInfo& account,
      AccountInfo* persisted_account) {
    return Steps(MakePrimaryAccountAvailable(account, persisted_account),
                 Do([this]() {
                   signin::IdentityTestEnvironment* identity_test_env =
                       identity_test_environment_adaptor_->identity_test_env();
                   identity_test_env->SetInvalidRefreshTokenForPrimaryAccount();
                 }));
  }

  auto SetRefreshTokenForPrimaryAccount() {
    return Do([this]() {
      signin::IdentityTestEnvironment* identity_test_env =
          identity_test_environment_adaptor_->identity_test_env();
      identity_test_env->SetRefreshTokenForPrimaryAccount();
    });
  }

  auto MakePrimaryAccountAvailableWithValidRefreshToken(
      const AccountInfo& account,
      AccountInfo* persisted_account) {
    return Steps(MakePrimaryAccountAvailable(account, persisted_account),
                 SetRefreshTokenForPrimaryAccount());
  }

  auto DestroyAccountChooserController() {
    return Do([this]() { account_chooser_controller_.reset(); });
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          // Disable the sync service by injecting a test fake. The sync service
          // fails to start when overriding the DeviceInfoSyncService with a
          // test fake.
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

  std::unique_ptr<AccountChooserController> account_chooser_controller_;
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
};

IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       ShowAccountChooserAndChooseAccount) {
  AccountInfo expected_account =
      GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  content::WaiterHelper waiter;
  AccountChosenCallback on_account_chosen_callback =
      GetOnAccountChosenCallback(expected_account, waiter);
  RunTestSequence(CreateAccountChooserController(),
                  MakeAccountAvailable(expected_account),
                  GetAccount(std::move(on_account_chosen_callback)),
                  WaitForShow(AccountChooserView::kTopViewId),
                  WaitForShow(AccountChooserView::kSaveButtonId),
                  PressButton(AccountChooserView::kSaveButtonId),
                  Do([&waiter]() { EXPECT_TRUE(waiter.Wait()); }));
}

IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       ShowAccountChooserAndCancelFlow) {
  AccountInfo account = GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  content::WaiterHelper waiter;
  AccountChosenCallback on_account_chosen_callback =
      GetOnNoAccountChosenCallback(waiter);
  RunTestSequence(CreateAccountChooserController(),
                  MakeAccountAvailable(account),
                  GetAccount(std::move(on_account_chosen_callback)),
                  WaitForShow(AccountChooserView::kTopViewId),
                  WaitForShow(AccountChooserView::kCancelButtonId),
                  PressButton(AccountChooserView::kCancelButtonId),
                  WaitForHide(AccountChooserView::kTopViewId),
                  Do([&waiter]() { EXPECT_TRUE(waiter.Wait()); }));
}

IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       ShowAddAccountPopupNoAccountsAndCancelFlow) {
  content::WaiterHelper waiter;
  AccountChosenCallback on_account_chosen_callback =
      GetOnNoAccountChosenCallback(waiter);
  RunTestSequence(CreateAccountChooserController(),
                  GetAccount(std::move(on_account_chosen_callback)),
                  VerifyPopupOpened(), ClosePopup(), VerifyPopupClosed(),
                  Do([&waiter]() { EXPECT_TRUE(waiter.Wait()); }));
}

IN_PROC_BROWSER_TEST_F(
    AccountChooserControllerInteractiveUiTest,
    ClickAddAccountButtonOpensAddAccountPopupAndClosePopupDoesNotCancelFlow) {
  AccountInfo account = GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  RunTestSequence(CreateAccountChooserController(),
                  MakeAccountAvailable(account),
                  GetAccount(/*on_account_chosen_callback=*/base::DoNothing()),
                  WaitForShow(AccountChooserView::kTopViewId),
                  WaitForShow(AccountChooserView::kAddAccountButtonId),
                  PressButton(AccountChooserView::kAddAccountButtonId),
                  VerifyPopupOpened(), ClosePopup(), VerifyPopupClosed(),
                  EnsurePresent(AccountChooserView::kTopViewId));
}

// Steps:
// 1. Call GetAccount with one account.
// 2. Add an account.
// 3. Verify the popup window is shown.
// 4. Select the new account.
// 5. Verify the account is selected.
IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       AddAccountWithExistingAccount) {
  AccountInfo account = GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  AccountInfo chosen_account =
      GetTestAccount("fern", "test.com", /*gaia_id=*/2);
  content::WaiterHelper waiter;
  AccountChosenCallback on_account_chosen_callback =
      GetOnAccountChosenCallback(chosen_account, waiter);
  RunTestSequence(
      // 1. Call GetAccount with one account.
      CreateAccountChooserController(), MakeAccountAvailable(account),
      GetAccount(std::move(on_account_chosen_callback)),
      WaitForShow(AccountChooserView::kTopViewId),
      WaitForShow(AccountChooserView::kAddAccountButtonId),
      PressButton(AccountChooserView::kAddAccountButtonId), VerifyPopupOpened(),
      MakeAccountAvailableAndSimulateOnExtendedAccountInfoUpdated(
          chosen_account),
      VerifyPopupClosed(), WaitForShow(AccountChooserView::kTopViewId),
      WaitForShow(AccountChooserView::kSaveButtonId),
      PressButton(AccountChooserView::kSaveButtonId),
      // Verify the account is selected.
      Do([&waiter]() { EXPECT_TRUE(waiter.Wait()); }));
}

// Steps:
// 1. Call GetAccount with multiple accounts.
// 2. Remove an account.
// 3. Verify the account chooser is shown.
// 4. Verify the account is selected.
IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       RemoveOneAccountFromMultipleAccounts) {
  std::vector<AccountInfo> accounts =
      GetTestAccounts({"pothos", "fern"}, "test.com");
  // Populate the persisted accounts with two empty accounts so it is legal to
  // access the elements in RunTestSequence, even though the persisted_accounts
  // will be modified.
  std::vector<AccountInfo> persisted_accounts = {AccountInfo(), AccountInfo()};
  content::WaiterHelper waiter;
  std::vector<AccountInfo>* persisted_accounts_ptr = &persisted_accounts;
  AccountChosenCallback on_account_chosen_callback = base::BindLambdaForTesting(
      [persisted_accounts_ptr, &waiter](std::optional<AccountInfo> account) {
        ASSERT_TRUE(!persisted_accounts_ptr->empty());
        ASSERT_TRUE(account.has_value());
        // The first account should be selected because we removed the
        // second account.
        const AccountInfo& expected_account = persisted_accounts_ptr->at(0);
        EXPECT_EQ(account->full_name, expected_account.full_name);
        EXPECT_EQ(account->email, expected_account.email);
        waiter.OnEvent();
      });
  RunTestSequence(
      CreateAccountChooserController(),
      MakeAccountsAvailable(accounts, &persisted_accounts),
      GetAccount(std::move(on_account_chosen_callback)),
      WaitForShow(AccountChooserView::kTopViewId),
      RemoveAccountFromIdentityTestEnvAndSimulateOnRefreshTokenRemovedForAccount(
          // Arbitrarily remove the second account.
          &persisted_accounts[1]),
      WaitForShow(AccountChooserView::kTopViewId),
      WaitForShow(AccountChooserView::kSaveButtonId),
      PressButton(AccountChooserView::kSaveButtonId),
      // Verify the first account is selected.
      Do([&waiter]() { EXPECT_TRUE(waiter.Wait()); }));
}

// Steps:
// 1. Call GetAccount with one account.
// 2. Remove the account.
// 3. Verify the popup window is shown.
IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       RemoveAllAccounts) {
  AccountInfo account = GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  AccountInfo persisted_account;
  RunTestSequence(
      CreateAccountChooserController(),
      MakeAccountAvailable(account, &persisted_account),
      GetAccount(base::DoNothing()),
      WaitForShow(AccountChooserView::kTopViewId),
      RemoveAccountFromIdentityTestEnvAndSimulateOnRefreshTokenRemovedForAccount(
          // Arbitrarily remove the one account.
          &persisted_account),
      VerifyPopupOpened());
}

// Steps:
// 1. Call GetAccount with a signed out primary account.
// 2. Verify the popup window is shown.
// 3. Sign into the primary account.
// 4. Verify the account chooser is shown.
IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       SignedOutPrimaryAccount) {
  AccountInfo account = GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  AccountInfo persisted_primary_account;
  RunTestSequence(CreateAccountChooserController(),
                  MakePrimaryAccountAvailableWithInvalidRefreshToken(
                      account, &persisted_primary_account),
                  GetAccount(base::DoNothing()), VerifyPopupOpened(),
                  SetRefreshTokenForPrimaryAccount(),
                  WaitForShow(AccountChooserView::kTopViewId));
}

// Steps:
// 1. Call GetAccount with no accounts.
// 2. Sign into the primary account.
// 3. Verify the account chooser is shown.
IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       SignIntoPrimaryAccount) {
  AccountInfo account = GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  AccountInfo persisted_primary_account;
  RunTestSequence(CreateAccountChooserController(),
                  GetAccount(base::DoNothing()), VerifyPopupOpened(),
                  MakePrimaryAccountAvailableWithValidRefreshToken(
                      account, &persisted_primary_account),
                  WaitForShow(AccountChooserView::kTopViewId));
}

// Steps:
// 1. Call GetAccount with one account.
// 2. Destroy the account chooser controller.
IN_PROC_BROWSER_TEST_F(AccountChooserControllerInteractiveUiTest,
                       DestroyAccountChooserController) {
  AccountInfo account = GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  RunTestSequence(CreateAccountChooserController(),
                  MakeAccountAvailable(account),
                  GetAccount(/*on_account_chosen_callback=*/base::DoNothing()),
                  WaitForShow(AccountChooserView::kTopViewId),
                  DestroyAccountChooserController(),
                  WaitForHide(AccountChooserView::kTopViewId));
}

}  // namespace
}  // namespace save_to_drive
