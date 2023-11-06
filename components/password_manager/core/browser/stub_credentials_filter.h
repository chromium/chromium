// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_CREDENTIALS_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_CREDENTIALS_FILTER_H_

#include "components/password_manager/core/browser/credentials_filter.h"

namespace password_manager {

// Stub of the CredentialsFilter API, to be used in tests. This filter does
// not filter out anything.
class StubCredentialsFilter : public CredentialsFilter {
 public:
  StubCredentialsFilter();

  StubCredentialsFilter(const StubCredentialsFilter&) = delete;
  StubCredentialsFilter& operator=(const StubCredentialsFilter&) = delete;

  ~StubCredentialsFilter() override;

  // CredentialsFilter
  bool ShouldSave(const PasswordForm& form) const override;
  bool ShouldSaveGaiaPasswordHash(const PasswordForm& form) const override;
  bool ShouldSaveEnterprisePasswordHash(
      const PasswordForm& form) const override;
  bool IsSyncAccountEmail(const std::string& username) const override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_CREDENTIALS_FILTER_H_
