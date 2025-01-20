// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_INTERFACE_H_

#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

// Interface for `LoginDbDeprecationPasswordExporter` to allow mocking in tests.
class LoginDbDeprecationPasswordExporterInterface {
 public:
  virtual ~LoginDbDeprecationPasswordExporterInterface() = default;

  // Starts the export flow. It will first fetch the stored credentials
  // and when fetching completes the actual export will be automatically
  // started.
  virtual void Start(scoped_refptr<PasswordStoreInterface> password_store,
                     base::OnceClosure export_cleanup_calback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_LOGIN_DB_DEPRECATION_PASSWORD_EXPORTER_INTERFACE_H_
