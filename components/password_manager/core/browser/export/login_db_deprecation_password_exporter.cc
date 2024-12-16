// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
namespace password_manager {

namespace {
constexpr std::string_view kExportedPasswordsFileName = "ChromePasswords.csv";
}  // namespace

LoginDbDeprecationPasswordExporter::LoginDbDeprecationPasswordExporter(
    base::FilePath export_dir_path)
    : export_dir_path_(std::move(export_dir_path)) {}
LoginDbDeprecationPasswordExporter::~LoginDbDeprecationPasswordExporter() =
    default;

void LoginDbDeprecationPasswordExporter::Start(
    scoped_refptr<PasswordStoreInterface> password_store,
    base::OnceClosure export_cleanup_calback) {
  export_cleanup_callback_ = std::move(export_cleanup_calback);
  password_store->GetAutofillableLogins(weak_factory_.GetWeakPtr());
}

std::vector<CredentialUIEntry>
LoginDbDeprecationPasswordExporter::GetSavedCredentials() const {
  return passwords_;
}

void LoginDbDeprecationPasswordExporter::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
    OnExportComplete();
    return;
  }

  // This is only invoked once, since the export flow goverened by this
  // class is a one-time operation.
  CHECK(passwords_.empty());
  passwords_.reserve(absl::get<LoginsResult>(logins_or_error).size());
  for (const auto& password_form : absl::get<LoginsResult>(logins_or_error)) {
    passwords_.emplace_back(password_form);
  }
  if (passwords_.empty()) {
    OnExportComplete();
    return;
  }

  // TODO(crbug.com/378650395): Pass the all the callbacks in the constructor.
  exporter_ = std::make_unique<PasswordManagerExporter>(
      this, base::DoNothing(),
      base::BindOnce(&LoginDbDeprecationPasswordExporter::OnExportComplete,
                     weak_factory_.GetWeakPtr()));
  exporter_->PreparePasswordsForExport();
  exporter_->SetDestination(
      export_dir_path_.Append(FILE_PATH_LITERAL(kExportedPasswordsFileName)));
}

void LoginDbDeprecationPasswordExporter::OnExportComplete() {
  // TODO(crbug.com/378650395): Handle success/failure.
  std::move(export_cleanup_callback_).Run();
  // The callback above destroys `this`.
}

}  // namespace password_manager
