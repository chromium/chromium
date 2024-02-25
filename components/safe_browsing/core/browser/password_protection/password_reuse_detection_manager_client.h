// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "build/build_config.h"

namespace autofill {
class LogManager;
}  // namespace autofill

namespace password_manager {
class PasswordReuseManager;
namespace metrics_util {
enum class PasswordType;
}
struct MatchingReusedCredential;
}  // namespace password_manager

namespace safe_browsing {

// An abstraction of operations that depend on the embedders (e.g. Chrome)
// environment. It manages password reuse detection.
class PasswordReuseDetectionManagerClient {
 public:
  PasswordReuseDetectionManagerClient() = default;

  PasswordReuseDetectionManagerClient(
      const PasswordReuseDetectionManagerClient&) = delete;
  PasswordReuseDetectionManagerClient& operator=(
      const PasswordReuseDetectionManagerClient&) = delete;

  virtual ~PasswordReuseDetectionManagerClient() = default;

  // Records a Chrome Sync event that GAIA password reuse was detected.
  virtual void MaybeLogPasswordReuseDetectedEvent() = 0;

  // Returns a LogManager instance.
  virtual autofill::LogManager* GetLogManager() = 0;

  // Returns the PasswordReuseManager associated with this instance.
  virtual password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const = 0;

  // Checks if |username| matches sync account, and the history sync datatype is
  // enabled.
  virtual bool IsHistorySyncAccountEmail(const std::string& username) = 0;

  // Checks if password field detected on page.
  virtual bool IsPasswordFieldDetectedOnPage() = 0;

  // Checks the safe browsing reputation of the webpage where password reuse
  // happens. This is called by the PasswordReuseDetectionManager when a
  // protected password is typed on the wrong domain. This may trigger a
  // warning dialog if it looks like the page is phishy.
  // The |username| is the user name of the reused password. The user name
  // can be an email or a username for a non-GAIA or saved-password reuse. No
  // validation has been done on it. The |domain| is the origin of the webpage
  // where password reuse happens. The |reused_password_hash| is the hash of the
  // reused password.
  virtual void CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists,
      uint64_t reused_password_hash,
      const std::string& domain) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Informs `PasswordReuseDetectionManager` about reused passwords selected
  // from the AllPasswordsBottomSheet.
  virtual void OnPasswordSelected(const std::u16string& text) = 0;
#endif
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
