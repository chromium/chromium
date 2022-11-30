// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTION_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTION_MANAGER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace password_manager {

class PasswordManagerClient;

// Class for managing password reuse detection. Now it receives keystrokes and
// does nothing with them. TODO(crbug.com/657041): write other features of this
// class when they are implemented. This class is one per-tab.
class PasswordReuseDetectionManager : public PasswordReuseDetectorConsumer {
 public:
  explicit PasswordReuseDetectionManager(PasswordManagerClient* client);

  PasswordReuseDetectionManager(const PasswordReuseDetectionManager&) = delete;
  PasswordReuseDetectionManager& operator=(
      const PasswordReuseDetectionManager&) = delete;

  ~PasswordReuseDetectionManager() override;
  void DidNavigateMainFrame(const GURL& main_frame_url);
  void OnKeyPressedCommitted(const std::u16string& text);
#if BUILDFLAG(IS_ANDROID)
  void OnKeyPressedUncommitted(const std::u16string& text);
#endif
  void OnPaste(const std::u16string text);

  // PasswordReuseDetectorConsumer implementation
  void OnReuseCheckDone(
      bool is_reuse_found,
      size_t password_length,
      absl::optional<PasswordHashData> reused_protected_password_hash,
      const std::vector<MatchingReusedCredential>& matching_reused_credentials,
      int saved_passwords,
      const std::string& domain,
      uint64_t reused_password_hash) override;

  void SetClockForTesting(base::Clock* clock);

 private:
  void OnKeyPressed(const std::u16string& text, const bool is_committed);
  // Determines the type of password being reused.
  metrics_util::PasswordType GetReusedPasswordType(
      absl::optional<PasswordHashData> reused_protected_password_hash,
      size_t match_domain_count);

  void CheckStoresForReuse(const std::u16string& input);

  raw_ptr<PasswordManagerClient> client_;
  std::u16string input_characters_;
  GURL main_frame_url_;
  base::Time last_keystroke_time_;
  // Used to retrieve the current time, in base::Time units.
  raw_ptr<base::Clock> clock_;
  bool reuse_on_this_page_was_found_ = false;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTION_MANAGER_H_
