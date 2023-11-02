// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_LOGGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_LOGGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/gurl.h"

namespace autofill {
class LogManager;
}

namespace password_manager {

// A helper for logging Credential Management API calls to
// chrome://password-manager-internals.
class CredentialManagerLogger {
 public:
  explicit CredentialManagerLogger(autofill::LogManager*);
  CredentialManagerLogger(const CredentialManagerLogger&) = delete;
  CredentialManagerLogger& operator=(const CredentialManagerLogger&) = delete;
  ~CredentialManagerLogger();

  void LogRequestCredential(const url::Origin& url,
                            CredentialMediationRequirement mediation,
                            const std::vector<GURL>& federations);
  void LogSendCredential(const url::Origin& origin, CredentialType type);
  void LogStoreCredential(const url::Origin& origin, CredentialType type);
  void LogPreventSilentAccess(const url::Origin& origin);

 private:
  // The LogManager to which logs can be sent for display.
  const raw_ptr<autofill::LogManager> log_manager_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_LOGGER_H_
