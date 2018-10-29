// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_credentials_filter.h"

namespace password_manager {

StubCredentialsFilter::StubCredentialsFilter() = default;

StubCredentialsFilter::~StubCredentialsFilter() = default;

std::vector<std::unique_ptr<autofill::PasswordForm>>
StubCredentialsFilter::FilterResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) const {
  FilterResultsPtr(&results);
  return results;
}

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
    const PasswordFormManagerInterface& form_manager) const {}

bool StubCredentialsFilter::IsSyncAccountEmail(
    const std::string& username) const {
  return false;
}

void StubCredentialsFilter::FilterResultsPtr(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* results) const {}

}  // namespace password_manager
