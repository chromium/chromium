// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"

#include <variant>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {
void LogExportResult(LoginDbDeprecationExportResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.UPM.LoginDbDeprecationExport.Result", result);
}

void LogExportLatency(base::TimeDelta latency) {
  base::UmaHistogramMediumTimes(
      "PasswordManager.UPM.LoginDbDeprecationExport.Latency", latency);
}

}  // namespace

LoginDbDeprecationPasswordExporter::LoginDbDeprecationPasswordExporter(
    PrefService* pref_service,
    base::FilePath export_dir_path)
    : pref_service_(pref_service),
      export_dir_path_(std::move(export_dir_path)) {
  exporter_ = std::make_unique<PasswordManagerExporter>(
      this,
      base::BindRepeating(&LoginDbDeprecationPasswordExporter::OnExportProgress,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&LoginDbDeprecationPasswordExporter::OnExportComplete,
                     weak_factory_.GetWeakPtr()));
}

LoginDbDeprecationPasswordExporter::~LoginDbDeprecationPasswordExporter() =
    default;

void LoginDbDeprecationPasswordExporter::Start(
    scoped_refptr<PasswordStoreInterface> password_store,
    base::OnceClosure export_cleanup_calback) {
  password_store_ = password_store;
  export_cleanup_callback_ = std::move(export_cleanup_calback);
  start_time_ = base::Time::Now();
  password_store->GetAutofillableLogins(weak_factory_.GetWeakPtr());
}

std::vector<CredentialUIEntry>
LoginDbDeprecationPasswordExporter::GetSavedCredentials() const {
  return passwords_;
}

PasswordManagerExporter*
LoginDbDeprecationPasswordExporter::GetInternalExporterForTesting(
    base::PassKey<LoginDbDeprecationPasswordExporterTest>) {
  return exporter_.get();
}

void LoginDbDeprecationPasswordExporter::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError logins_or_error) {
  if (std::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
    OnExportCompleteWithResult(
        LoginDbDeprecationExportResult::kErrorFetchingPasswords);
    return;
  }

  // This is only invoked once, since the export flow goverened by this
  // class is a one-time operation.
  CHECK(passwords_.empty());
  passwords_.reserve(std::get<LoginsResult>(logins_or_error).size());
  for (const auto& password_form : std::get<LoginsResult>(logins_or_error)) {
    passwords_.emplace_back(password_form);
  }
  if (passwords_.empty()) {
    OnExportCompleteWithResult(LoginDbDeprecationExportResult::kNoPasswords);
    return;
  }

  exporter_->PreparePasswordsForExport();
  exporter_->SetDestination(
      export_dir_path_.Append(FILE_PATH_LITERAL(kExportedPasswordsFileName)));
}

void LoginDbDeprecationPasswordExporter::OnExportProgress(
    const PasswordExportInfo& export_info) {
  export_status_ = export_info.status;
}

void LoginDbDeprecationPasswordExporter::OnExportComplete() {
  LoginDbDeprecationExportResult result;
  switch (export_status_) {
    case ExportProgressStatus::kNotStarted:
    case ExportProgressStatus::kInProgress: {
      // `OnExportComplete` should only be called for completed export flows.
      NOTREACHED();
    }
    case ExportProgressStatus::kSucceeded: {
      result = LoginDbDeprecationExportResult::kSuccess;
      break;
    }
    case ExportProgressStatus::kFailedCancelled: {
      // There is no option that cancels this export flow.
      NOTREACHED();
    }
    case ExportProgressStatus::kFailedWrite: {
      result = LoginDbDeprecationExportResult::kFileWriteError;
      break;
    }
  };
  OnExportCompleteWithResult(result);
}

void LoginDbDeprecationPasswordExporter::OnExportCompleteWithResult(
    LoginDbDeprecationExportResult result) {
  LogExportResult(result);

  // If the export wasn't successful and there are passwords to export,
  // it will be re-attempted on the next startup.
  if (result == LoginDbDeprecationExportResult::kSuccess) {
    LogExportLatency(base::Time::Now() - start_time_);
    pref_service_->SetBoolean(prefs::kUpmUnmigratedPasswordsExported, true);
    password_store_->RemoveLoginsCreatedBetween(FROM_HERE, base::Time(),
                                                base::Time::Max());
  } else if (result == LoginDbDeprecationExportResult::kNoPasswords) {
    // Nothing to export, so the export can be marked as done.
    pref_service_->SetBoolean(
        password_manager::prefs::kUpmUnmigratedPasswordsExported, true);
  }

  std::move(export_cleanup_callback_).Run();
  // The callback above destroys `this`.
}

}  // namespace password_manager
