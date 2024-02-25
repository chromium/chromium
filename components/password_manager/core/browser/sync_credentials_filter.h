// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

struct PasswordForm;

// The sync- and GAIA- aware implementation of the filter.
class SyncCredentialsFilter : public CredentialsFilter {
 public:
  // Implements protection of sync credentials. Uses |client| to get the last
  // committed entry URL for a check against GAIA reauth site.
  explicit SyncCredentialsFilter(PasswordManagerClient* client);

  SyncCredentialsFilter(const SyncCredentialsFilter&) = delete;
  SyncCredentialsFilter& operator=(const SyncCredentialsFilter&) = delete;

  ~SyncCredentialsFilter() override;

  // CredentialsFilter
  bool ShouldSave(const PasswordForm& form) const override;
  bool ShouldSaveGaiaPasswordHash(const PasswordForm& form) const override;
  bool ShouldSaveEnterprisePasswordHash(
      const PasswordForm& form) const override;
  bool IsSyncAccountEmail(const std::string& username) const override;

 private:
  const raw_ptr<PasswordManagerClient> client_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_
