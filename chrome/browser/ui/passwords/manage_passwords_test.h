// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_

#include <memory>
#include <vector>

#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/common/form_data.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"

class ManagePasswordsUIController;

enum class SyncConfiguration {
  // There is no sync consent and no sync types are synced.
  kNotSyncing = 0,
  // There is sync consent.
  kSyncing = 1,
  // There is no sync consent, but passwords are saved in the account storage.
  kAccountStorageOnly = 2,
  kMaxValue = kAccountStorageOnly,
};

// Test class for the various password management view bits and pieces. Provides
// some helper methods to poke at the bubble, icon, and controller's state.
class ManagePasswordsTest : public InteractiveBrowserTest {
 public:
  ManagePasswordsTest();

  ManagePasswordsTest(const ManagePasswordsTest&) = delete;
  ManagePasswordsTest& operator=(const ManagePasswordsTest&) = delete;

  ~ManagePasswordsTest() override;

  // InteractiveBrowserTest:
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  // Execute the browser command to open the manage passwords bubble.
  void ExecuteManagePasswordsCommand();

  // Put the controller, icon, and bubble into a managing-password state.
  // TODO(crbug.com/41491760): Make password form url stable without having to
  // override it.
  void SetupManagingPasswords(const GURL& password_form_url = GURL());

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
  // configures the Sync service to sync passwords. |is_account_storage_enabled|
  // enables account storage for the user.
  void ConfigurePasswordSync(SyncConfiguration configuration);

  // Get samples for |histogram|.
  std::unique_ptr<base::HistogramSamples> GetSamples(const char* histogram);

  password_manager::PasswordForm* test_form() { return &password_form_; }

  // Get the UI controller for the current WebContents.
  ManagePasswordsUIController* GetController();

 protected:
  // Creates a form manager using the given password password stores.
  // If |profile_store| is nullptr, password_manager::StubFormSaver is used for
  // the profile store. If |account_store| is nullptr, a nullptr
  // password_manager::FormSaver is used for the account store.
  std::unique_ptr<password_manager::PasswordFormManager> CreateFormManager(
      password_manager::PasswordStoreInterface* profile_store = nullptr,
      password_manager::PasswordStoreInterface* account_store = nullptr);

 private:
  password_manager::PasswordForm password_form_;
  password_manager::PasswordForm insecure_credential_;
  base::HistogramTester histogram_tester_;
  password_manager::StubPasswordManagerClient client_;
  password_manager::StubPasswordManagerDriver driver_;
  password_manager::FakeFormFetcher fetcher_;

  base::CallbackListSubscription create_services_subscription_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_
