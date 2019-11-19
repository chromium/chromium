// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_credentials_filter.h"

namespace password_manager {

StubCredentialsFilter::StubCredentialsFilter() = default;

StubCredentialsFilter::~StubCredentialsFilter() = default;

bool StubCredentialsFilter::ShouldSave(
    const autofill::PasswordForm& form) const {
  return true;
}

bool StubCredentialsFilter::ShouldSaveGaiaPasswordHash(
    const autofill::PasswordForm& form) const {
  return false;
}

bool StubCredentialsFilter::ShouldSaveEnterprisePasswordHash(
    const autofill::PasswordForm& form) const {
  return false;
}

void StubCredentialsFilter::ReportFormLoginSuccess(
    const PasswordFormManager& form_manager) const {}

bool StubCredentialsFilter::IsSyncAccountEmail(
    const std::string& username) const {
  return false;
}

}  // namespace password_manager
