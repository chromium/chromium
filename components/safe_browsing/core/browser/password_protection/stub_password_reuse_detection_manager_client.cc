// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/core/browser/password_protection/stub_password_reuse_detection_manager_client.h"
namespace safe_browsing {

StubPasswordReuseDetectionManagerClient::
    StubPasswordReuseDetectionManagerClient() = default;

StubPasswordReuseDetectionManagerClient::
    ~StubPasswordReuseDetectionManagerClient() = default;

autofill::LogManager* StubPasswordReuseDetectionManagerClient::GetLogManager() {
  return &log_manager_;
}

void StubPasswordReuseDetectionManagerClient::CheckProtectedPasswordEntry(
    password_manager::metrics_util::PasswordType reused_password_type,
    const std::string& username,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    bool password_field_exists,
    uint64_t reused_password_hash,
    const std::string& domain) {}

password_manager::PasswordReuseManager*
StubPasswordReuseDetectionManagerClient::GetPasswordReuseManager() const {
  return nullptr;
}

bool StubPasswordReuseDetectionManagerClient::IsHistorySyncAccountEmail(
    const std::string& username) {
  return false;
}

bool StubPasswordReuseDetectionManagerClient::IsPasswordFieldDetectedOnPage() {
  return false;
}

void StubPasswordReuseDetectionManagerClient::
    MaybeLogPasswordReuseDetectedEvent() {}

}  // namespace safe_browsing
