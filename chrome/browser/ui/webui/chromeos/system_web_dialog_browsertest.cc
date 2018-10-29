// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.mojom.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/account_id/account_id.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "url/gurl.h"

namespace {

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";

// Returns whether a system modal window (e.g. modal dialog) is open. Blocks
// until the ash service responds.
bool IsSystemModalWindowOpen() {
  // Wait for window visibility to stabilize.
  aura::test::WaitForAllChangesToComplete();

  // Connect to the ash test interface.
  ash::mojom::ShellTestApiPtr shell_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_test_api);
  ash::mojom::ShellTestApiAsyncWaiter waiter(shell_test_api.get());
  bool modal_open = false;
  waiter.IsSystemModalWindowOpen(&modal_open);
  return modal_open;
}

class SystemWebDialogTest : public chromeos::LoginManagerTest {
 public:
  SystemWebDialogTest()
      : LoginManagerTest(false, true /* should_initialize_webui */) {}
  ~SystemWebDialogTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemWebDialogTest);
};

class MockSystemWebDialog : public chromeos::SystemWebDialogDelegate {
 public:
  MockSystemWebDialog()
      : SystemWebDialogDelegate(GURL(chrome::kChromeUIVersionURL),
                                base::string16()) {}
  ~MockSystemWebDialog() override = default;

  std::string GetDialogArgs() const override { return std::string(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemWebDialog);
};

}  // namespace

// Verifies that system dialogs are modal before login (e.g. during OOBE).
IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, ModalTest) {
  chromeos::SystemWebDialogDelegate* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_TRUE(IsSystemModalWindowOpen());
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, PRE_NonModalTest) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that system dialogs are not modal after login.
IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, NonModalTest) {
  LoginUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  chromeos::SystemWebDialogDelegate* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_FALSE(IsSystemModalWindowOpen());
}
