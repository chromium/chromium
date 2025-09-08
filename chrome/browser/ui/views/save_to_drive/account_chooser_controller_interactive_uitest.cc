// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_controller.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_test_util.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
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

  AccountInfo MakeAccountAvailableInIdentityTestEnv(
      const AccountInfo& account) {
    signin::IdentityTestEnvironment* identity_test_env =
        identity_test_environment_adaptor_->identity_test_env();
    AccountInfo persisted_account =
        identity_test_env->MakeAccountAvailable(account.email);
    persisted_account.full_name = account.full_name;
    persisted_account.account_image = account.account_image;
    identity_test_env->UpdateAccountInfoForAccount(persisted_account);
    signin::SimulateAccountImageFetch(
        identity_test_env->identity_manager(), persisted_account.account_id,
        "https://avatar.com/avatar.png", persisted_account.account_image);
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
        CheckResult(
            []() -> size_t { return BrowserList::GetInstance()->size(); }, 2u,
            "Expect two browsers."),
        Check(
            []() {
              return BrowserList::GetInstance()->get(1)->is_type_popup();
            },
            "Expect second browser is popup."));
  }

  // Must be called after VerifyPopupOpened().
  auto ClosePopup() {
    return Do([]() { BrowserList::GetInstance()->get(1)->window()->Close(); });
  }

  auto VerifyPopupClosed() {
    return CheckResult(
        []() -> size_t { return BrowserList::GetInstance()->size(); }, 1u,
        "Expect one browser.");
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

}  // namespace
}  // namespace save_to_drive
