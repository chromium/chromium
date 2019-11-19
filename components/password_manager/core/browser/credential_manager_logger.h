// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_LOGGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_LOGGER_H_

#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/gurl.h"

namespace autofill {
class LogManager;
}

namespace password_manager {

// A helper for logging Credential Manager API calls to
// chrome://password-manager-internals.
class CredentialManagerLogger {
 public:
  explicit CredentialManagerLogger(const autofill::LogManager*);
  ~CredentialManagerLogger();

  void LogRequestCredential(const GURL& url,
                            CredentialMediationRequirement mediation,
                            const std::vector<GURL>& federations);
  void LogSendCredential(const GURL& url, CredentialType type);
  void LogStoreCredential(const GURL& url, CredentialType type);
  void LogPreventSilentAccess(const GURL& url);

 private:
  // The LogManager to which logs can be sent for display.
  const autofill::LogManager* const log_manager_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerLogger);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_LOGGER_H_
