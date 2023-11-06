// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_credentials_filter.h"

namespace password_manager {

StubCredentialsFilter::StubCredentialsFilter() = default;

StubCredentialsFilter::~StubCredentialsFilter() = default;

bool StubCredentialsFilter::ShouldSave(const PasswordForm& form) const {
  return true;
}

bool StubCredentialsFilter::ShouldSaveGaiaPasswordHash(
    const PasswordForm& form) const {
  return false;
}

bool StubCredentialsFilter::ShouldSaveEnterprisePasswordHash(
    const PasswordForm& form) const {
  return false;
}

bool StubCredentialsFilter::IsSyncAccountEmail(
    const std::string& username) const {
  return false;
}

}  // namespace password_manager
