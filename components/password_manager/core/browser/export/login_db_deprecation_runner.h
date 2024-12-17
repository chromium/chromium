// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_RUNNER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_RUNNER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

// Owns and runs the pre-deprecation password export on Android.
// Once the export is done it will destroy the exporter and clear the memory.
// The service will only be instantiated for clients whose passwords couldn't
// be migrated to GMS Core.
class LoginDbDeprecationRunner : public KeyedService {
 public:
  explicit LoginDbDeprecationRunner(PrefService* pref_service,
                                    base::FilePath export_dir);
  LoginDbDeprecationRunner(const LoginDbDeprecationRunner&) = delete;
  LoginDbDeprecationRunner& operator=(const LoginDbDeprecationRunner&) = delete;
  ~LoginDbDeprecationRunner() override;

  void StartExport(scoped_refptr<PasswordStoreInterface> password_store);

 private:
  void ExportFinished();

  std::unique_ptr<LoginDbDeprecationPasswordExporter> exporter_;

  base::WeakPtrFactory<LoginDbDeprecationRunner> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_RUNNER_H_
