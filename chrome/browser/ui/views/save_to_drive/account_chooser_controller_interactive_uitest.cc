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

  auto GetAccount(base::OnceCallback<void(std::optional<AccountInfo>)>
                      on_account_selected_callback) {
    return Do([this, on_account_selected_callback =
                         std::move(on_account_selected_callback)]() mutable {
      account_chooser_controller_->GetAccount(
          std::move(on_account_selected_callback));
    });
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
                       ShowAccountChooserOneAccount) {
  AccountInfo expected_account =
      GetTestAccount("pothos", "test.com", /*gaia_id=*/1);
  RunTestSequence(
      CreateAccountChooserController(), MakeAccountAvailable(expected_account),
      GetAccount(/*on_account_selected_callback=*/base::DoNothing()),
      WaitForShow(AccountChooserView::kTopViewId),
      EnsurePresent(AccountChooserView::kTopViewId));
}

}  // namespace
}  // namespace save_to_drive
