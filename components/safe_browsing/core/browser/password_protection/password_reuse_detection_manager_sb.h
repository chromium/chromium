// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_SB_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_SB_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace safe_browsing {

// This is a placeholder class to compile the new client files.
// TODO(https://crbug.com/1322599): Rename the class name back to
// PasswordReuseDetectionManager and delete the obsolete files in the password
// manager directory.

// TODO(https://crbug.com/1422140): Refactor the
// password_reuse_detection_manager files. Class for managing password reuse
// detection. Now it receives keystrokes and does nothing with them.
// TODO(crbug.com/657041): write other features of this class when they are
// implemented. This class is one per-tab.
class PasswordReuseDetectionManagerSB
    : public password_manager::PasswordReuseDetectorConsumer {
 public:
  explicit PasswordReuseDetectionManagerSB(
      password_manager::PasswordManagerClient* client);

  PasswordReuseDetectionManagerSB(const PasswordReuseDetectionManagerSB&) =
      delete;
  PasswordReuseDetectionManagerSB& operator=(
      const PasswordReuseDetectionManagerSB&) = delete;

  ~PasswordReuseDetectionManagerSB() override;

  // Updates members based on whether the user navigated to another main frame
  // or not.
  void DidNavigateMainFrame(const GURL& main_frame_url);

  // Checks reuse for the committed texts.
  void OnKeyPressedCommitted(const std::u16string& text);

#if BUILDFLAG(IS_ANDROID)
  // Checks reuse for the uncommitted texts.
  void OnKeyPressedUncommitted(const std::u16string& text);
#endif

  // Performs password reuse check when a string is pasted.
  void OnPaste(std::u16string text);

  // PasswordReuseDetectorConsumer implementation
  void OnReuseCheckDone(
      bool is_reuse_found,
      size_t password_length,
      absl::optional<password_manager::PasswordHashData>
          reused_protected_password_hash,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      int saved_passwords,
      const std::string& domain,
      uint64_t reused_password_hash) override;

  void SetClockForTesting(base::Clock* clock);

 private:
  void OnKeyPressed(const std::u16string& text, bool is_committed);
  // Determines the type of password being reused.
  password_manager::metrics_util::PasswordType GetReusedPasswordType(
      absl::optional<password_manager::PasswordHashData>
          reused_protected_password_hash,
      size_t match_domain_count);

  void CheckStoresForReuse(const std::u16string& input);

  // A client to handle password reuse detection logic.
  raw_ptr<password_manager::PasswordManagerClient> client_;
  // A buffer that stores keystrokes.
  std::u16string input_characters_;
  // The url of the current main frame.
  GURL main_frame_url_;
  // Indicates when the last keystroke was detected.
  base::Time last_keystroke_time_;
  // Used to retrieve the current time, in base::Time units.
  raw_ptr<base::Clock> clock_;
  // Helps determine whether or not to check reuse based on if a reuse was
  // already found.
  bool reuse_on_this_page_was_found_ = false;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_SB_H_
