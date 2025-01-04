// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/login_db_deprecation_runner.h"

#include "base/memory/scoped_refptr.h"

namespace password_manager {

LoginDbDeprecationRunner::LoginDbDeprecationRunner(PrefService* pref_service,
                                                   base::FilePath export_dir)
    : exporter_(
          std::make_unique<LoginDbDeprecationPasswordExporter>(pref_service,
                                                               export_dir)) {}

LoginDbDeprecationRunner::~LoginDbDeprecationRunner() = default;

void LoginDbDeprecationRunner::StartExport(
    scoped_refptr<PasswordStoreInterface> password_store) {
  // TODO(crbug.com/378650395): Delay the export by a configurable amount
  // of time.
  CHECK(exporter_);
  exporter_->Start(password_store,
                   base::BindOnce(&LoginDbDeprecationRunner::ExportFinished,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void LoginDbDeprecationRunner::ExportFinished() {
  exporter_.reset();
}

}  // namespace password_manager
