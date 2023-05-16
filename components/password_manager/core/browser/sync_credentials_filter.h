// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

struct PasswordForm;

// The sync- and GAIA- aware implementation of the filter.
class SyncCredentialsFilter : public CredentialsFilter {
 public:
  using SyncServiceFactoryFunction =
      base::RepeatingCallback<const syncer::SyncService*(void)>;

  // Implements protection of sync credentials. Uses |client| to get the last
  // commited entry URL for a check against GAIA reauth site. Uses the factory
  // function repeatedly to get the sync service to pass to sync_util methods.
  SyncCredentialsFilter(
      PasswordManagerClient* client,
      SyncServiceFactoryFunction sync_service_factory_function);

  SyncCredentialsFilter(const SyncCredentialsFilter&) = delete;
  SyncCredentialsFilter& operator=(const SyncCredentialsFilter&) = delete;

  ~SyncCredentialsFilter() override;

  // CredentialsFilter
  bool ShouldSave(const PasswordForm& form) const override;
  bool ShouldSaveGaiaPasswordHash(const PasswordForm& form) const override;
  bool ShouldSaveEnterprisePasswordHash(
      const PasswordForm& form) const override;
  void ReportFormLoginSuccess(
      const PasswordFormManager& form_manager) const override;
  bool IsSyncAccountEmail(const std::string& username) const override;

 private:
  const raw_ptr<PasswordManagerClient> client_;

  const SyncServiceFactoryFunction sync_service_factory_function_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_
