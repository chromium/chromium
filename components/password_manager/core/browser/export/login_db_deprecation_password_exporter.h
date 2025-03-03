// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter_interface.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/passwords_provider.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LoginDbDeprecationExportResult)
enum class LoginDbDeprecationExportResult {
  kSuccess = 0,
  kNoPasswords = 1,
  kErrorFetchingPasswords = 2,
  kFileWriteError = 3,

  kMaxValue = kFileWriteError,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:LoginDbDeprecationExportResult)

inline constexpr std::string_view kExportedPasswordsFileName =
    "ChromePasswords.csv";

// Directs exporting the passwords from the `LoginDatabase` to a CSV stored
// in the same place to allow for database deprecation.
class LoginDbDeprecationPasswordExporter
    : public LoginDbDeprecationPasswordExporterInterface,
      public PasswordStoreConsumer,
      public PasswordsProvider {
 public:
  explicit LoginDbDeprecationPasswordExporter(PrefService* pref_service,
                                              base::FilePath export_dir_path);
  LoginDbDeprecationPasswordExporter(
      const LoginDbDeprecationPasswordExporter&) = delete;
  LoginDbDeprecationPasswordExporter& operator=(
      const LoginDbDeprecationPasswordExporter&) = delete;
  ~LoginDbDeprecationPasswordExporter() override;

  void Start(scoped_refptr<PasswordStoreInterface> password_store,
             base::OnceClosure export_cleanup_calback) override;

  // Allows the `PasswordManagerExporter` to retrieve the saved credentials
  // after `this` receives them. Not a necessary pattern for this use-case
  // but one which the `PasswordManagerExport` expects, since it's usually
  // used in UI applications where another class holds the credentials.
  std::vector<CredentialUIEntry> GetSavedCredentials() const override;

  PasswordManagerExporter* GetInternalExporterForTesting(
      base::PassKey<class LoginDbDeprecationPasswordExporterTest>);

 private:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  // Reports on the progress of the export flow.
  void OnExportProgress(const PasswordExportInfo& export_info);

  // Called when the `exporter_` completes all the export operations,
  // irrespective of whether the export succeeded.
  void OnExportComplete();

  // Called when the export finishes (from `OnExportComplete`),
  // or before it starts if the passwords to export could not be fetched.
  void OnExportCompleteWithResult(LoginDbDeprecationExportResult result);

  // Used to delete the passwords after successful export.
  scoped_refptr<PasswordStoreInterface> password_store_;

  // Callback to invoke when ALL the export operations finished. It will clean
  // up `this`.
  base::OnceClosure export_cleanup_callback_;

  // Serializes the passwords and writes them to a CSV file.
  std::unique_ptr<PasswordManagerExporter> exporter_;

  // Stores the saved credentials.
  std::vector<CredentialUIEntry> passwords_;

  // Used to store the export completion status in a pref.
  raw_ptr<PrefService> pref_service_;

  // Path where the exported CSV will be written. It should be the same as the
  // login db path.
  base::FilePath export_dir_path_;

  // The last reported status of the export flow.
  ExportProgressStatus export_status_{ExportProgressStatus::kNotStarted};

  base::Time start_time_;

  base::WeakPtrFactory<LoginDbDeprecationPasswordExporter> weak_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_H_
