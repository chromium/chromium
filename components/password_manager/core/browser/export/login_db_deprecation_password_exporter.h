// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/passwords_provider.h"

namespace password_manager {

// Directs exporting the passwords from the `LoginDatabase` to a CSV stored
// in the same place to allow for database deprecation.
class LoginDbDeprecationPasswordExporter : public PasswordStoreConsumer,
                                           public PasswordsProvider {
 public:
  explicit LoginDbDeprecationPasswordExporter(base::FilePath export_dir_path);
  LoginDbDeprecationPasswordExporter(
      const LoginDbDeprecationPasswordExporter&) = delete;
  LoginDbDeprecationPasswordExporter& operator=(
      const LoginDbDeprecationPasswordExporter&) = delete;
  ~LoginDbDeprecationPasswordExporter() override;

  void Start(PasswordStoreInterface* password_store);

  // Allows the `PasswordManagerExporter` to retrieve the saved credentials
  // after `this` receives them. Not a necessary pattern for this use-case
  // but one which the `PasswordManagerExport` expects, since it's usually
  // used in UI applications where another class holds the credentials.
  std::vector<CredentialUIEntry> GetSavedCredentials() const override;

 private:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  // Serializes the passwords and writes them to a CSV file.
  std::unique_ptr<PasswordManagerExporter> exporter_;

  // Stores the saved credentials.
  std::vector<CredentialUIEntry> passwords_;

  // Path where the exported CSV will be written. It should be the same as the
  // login db path.
  base::FilePath export_dir_path_;

  base::WeakPtrFactory<LoginDbDeprecationPasswordExporter> weak_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_H_
