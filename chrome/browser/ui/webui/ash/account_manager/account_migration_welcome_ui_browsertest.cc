// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_ui.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/account_manager_core/chromeos/fake_account_manager_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using testing::Optional;
using testing::StrEq;

constexpr char kReauthEmail[] = "migration-welcome-user@example.com";
constexpr char kReauthenticateAccountMessage[] = "reauthenticateAccount";

}  // namespace

class AccountMigrationWelcomeUITest : public InProcessBrowserTest {
 public:
  AccountMigrationWelcomeUITest() = default;
  AccountMigrationWelcomeUITest(const AccountMigrationWelcomeUITest&) = delete;
  AccountMigrationWelcomeUITest& operator=(
      const AccountMigrationWelcomeUITest&) = delete;
  ~AccountMigrationWelcomeUITest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(GetProfile()));
    web_ui_.set_web_contents(web_contents_.get());
    account_migration_welcome_ui_ =
        std::make_unique<AccountMigrationWelcomeUI>(&web_ui_);
  }

  void TearDownOnMainThread() override {
    account_migration_welcome_ui_.reset();
    web_ui_.set_web_contents(nullptr);
    web_contents_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<AccountMigrationWelcomeUI> account_migration_welcome_ui_;
};

IN_PROC_BROWSER_TEST_F(AccountMigrationWelcomeUITest,
                       ReauthenticateAccountOpensReauthDialog) {
  base::HistogramTester histogram_tester;
  Profile* profile = GetProfile();
  crosapi::AccountManagerMojoService* account_manager_mojo_service =
      AccountManagerFactory::Get()->GetAccountManagerMojoService(
          profile->GetPath().value());
  ASSERT_TRUE(account_manager_mojo_service);

  auto fake_account_manager_ui = std::make_unique<FakeAccountManagerUI>();
  FakeAccountManagerUI* fake_account_manager_ui_ptr =
      fake_account_manager_ui.get();
  account_manager_mojo_service->SetAccountManagerUI(
      std::move(fake_account_manager_ui));

  base::ListValue args;
  args.Append(kReauthEmail);
  web_ui()->HandleReceivedMessage(kReauthenticateAccountMessage, args);

  EXPECT_EQ(1, fake_account_manager_ui_ptr
                   ->show_account_reauthentication_dialog_calls());
  EXPECT_THAT(fake_account_manager_ui_ptr->last_reauth_email(),
              Optional(StrEq(kReauthEmail)));
  EXPECT_EQ(0,
            fake_account_manager_ui_ptr->show_account_addition_dialog_calls());
  EXPECT_EQ(0,
            fake_account_manager_ui_ptr->show_manage_accounts_settings_calls());
  histogram_tester.ExpectUniqueSample(
      account_manager::AccountManagerFacade::kAccountAdditionSource,
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kAccountManagerMigrationWelcomeScreen,
      /*expected_count=*/1);

  fake_account_manager_ui_ptr->CloseDialog();
}

}  // namespace ash
