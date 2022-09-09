// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_

#include <memory>
#include <vector>

#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/form_data.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"

class ManagePasswordsUIController;

// Test class for the various password management view bits and pieces. Provides
// some helper methods to poke at the bubble, icon, and controller's state.
class ManagePasswordsTest : public InProcessBrowserTest {
 public:
  ManagePasswordsTest();

  ManagePasswordsTest(const ManagePasswordsTest&) = delete;
  ManagePasswordsTest& operator=(const ManagePasswordsTest&) = delete;

  ~ManagePasswordsTest() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  // Execute the browser command to open the manage passwords bubble.
  void ExecuteManagePasswordsCommand();

  // Put the controller, icon, and bubble into a managing-password state.
  void SetupManagingPasswords();

  // Put the controller, icon, and bubble into the confirmation state.
  void SetupAutomaticPassword();

  // Put the controller, icon, and bubble into a pending-password state.
  void SetupPendingPassword();

  // Put the controller, icon, and bubble into an auto sign-in state.
  void SetupAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials);

  // Put the controller, icon, and bubble into the state with 0 compromised
  // passwords saved.
  void SetupSafeState();

  // Put the controller, icon, and bubble into the "More problems to fix" state.
  void SetupMoreToFixState();

  // Put the controller, icon, and bubble into a moving-password state.
  void SetupMovingPasswords();

  // Always configures a signed-in user, and when |is_enabled| is true, it also
  // configures the Sync service to sync passwords.
  void ConfigurePasswordSync(bool is_enabled);

  // Get samples for |histogram|.
  std::unique_ptr<base::HistogramSamples> GetSamples(const char* histogram);

  password_manager::PasswordForm* test_form() { return &password_form_; }

  // Get the UI controller for the current WebContents.
  ManagePasswordsUIController* GetController();

  MOCK_METHOD1(OnChooseCredential,
               void(const password_manager::CredentialInfo&));

 private:
  std::unique_ptr<password_manager::PasswordFormManager> CreateFormManager();

  password_manager::PasswordForm password_form_;
  password_manager::PasswordForm insecure_credential_;
  base::HistogramTester histogram_tester_;
  password_manager::StubPasswordManagerClient client_;
  password_manager::StubPasswordManagerDriver driver_;
  password_manager::FakeFormFetcher fetcher_;

  base::CallbackListSubscription create_services_subscription_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_
