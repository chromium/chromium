// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_STUB_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_STUB_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_

#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager_client.h"

namespace safe_browsing {

// Use this class as a base for mock or test clients to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs.  Do NOT use in production, only use in tests.
class StubPasswordReuseDetectionManagerClient
    : public PasswordReuseDetectionManagerClient {
 public:
  StubPasswordReuseDetectionManagerClient();
  StubPasswordReuseDetectionManagerClient(
      const StubPasswordReuseDetectionManagerClient&) = delete;
  StubPasswordReuseDetectionManagerClient& operator=(
      const StubPasswordReuseDetectionManagerClient&) = delete;
  ~StubPasswordReuseDetectionManagerClient() override;

  // PasswordReuseDetectionManagerClient:
  password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const override;
  autofill::LogManager* GetLogManager() override;
  bool IsHistorySyncAccountEmail(const std::string& username) override;
  bool IsPasswordFieldDetectedOnPage() override;
  void CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists,
      uint64_t reused_password_hash,
      const std::string& domain) override;
  void MaybeLogPasswordReuseDetectedEvent() override;

 private:
  autofill::StubLogManager log_manager_;
};
}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_STUB_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
