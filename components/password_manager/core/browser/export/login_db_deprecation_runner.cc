// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/login_db_deprecation_runner.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"
#include "components/password_manager/core/browser/features/password_features.h"
namespace password_manager {

void LogExportProgress(LoginDbDeprecationExportProgress progress) {
  base::UmaHistogramEnumeration(
      "PasswordManager.UPM.LoginDbDeprecationExport.Progress", progress);
}

LoginDbDeprecationRunner::LoginDbDeprecationRunner(
    std::unique_ptr<LoginDbDeprecationPasswordExporterInterface> exporter)
    : exporter_(std::move(exporter)) {}

LoginDbDeprecationRunner::~LoginDbDeprecationRunner() = default;

void LoginDbDeprecationRunner::StartExportWithDelay(
    scoped_refptr<PasswordStoreInterface> password_store) {
  CHECK(exporter_);
  LogExportProgress(LoginDbDeprecationExportProgress::kScheduled);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LoginDbDeprecationRunner::StartExport,
                     weak_ptr_factory_.GetWeakPtr(), password_store),
      base::Seconds(
          password_manager::features::kLoginDbDeprecationExportDelay.Get()));
}

void LoginDbDeprecationRunner::StartExport(
    scoped_refptr<PasswordStoreInterface> password_store) {
  LogExportProgress(LoginDbDeprecationExportProgress::kStarted);
  exporter_->Start(password_store,
                   base::BindOnce(&LoginDbDeprecationRunner::ExportFinished,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void LoginDbDeprecationRunner::ExportFinished() {
  LogExportProgress(LoginDbDeprecationExportProgress::kFinished);
  exporter_.reset();
}

}  // namespace password_manager
