// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager_client.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace safe_browsing {

// TODO(crbug.com/40896734): Refactor the
// password_reuse_detection_manager files. Class for managing password reuse
// detection. Now it receives keystrokes and does nothing with them.
// PasswordReuseDetectionManager is instantiated once one per WebContents.
class PasswordReuseDetectionManager final
    : public password_manager::PasswordReuseDetectorConsumer {
 public:
  explicit PasswordReuseDetectionManager(
      PasswordReuseDetectionManagerClient* client);

  PasswordReuseDetectionManager(const PasswordReuseDetectionManager&) = delete;
  PasswordReuseDetectionManager& operator=(
      const PasswordReuseDetectionManager&) = delete;

  ~PasswordReuseDetectionManager() override;

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
      std::optional<password_manager::PasswordHashData>
          reused_protected_password_hash,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      int saved_passwords,
      const std::string& domain,
      uint64_t reused_password_hash) override;

  base::WeakPtr<PasswordReuseDetectorConsumer> AsWeakPtr() override;

  void SetClockForTesting(base::Clock* clock);

 private:
  void OnKeyPressed(const std::u16string& text, bool is_committed);
  // Determines the type of password being reused.
  password_manager::metrics_util::PasswordType GetReusedPasswordType(
      std::optional<password_manager::PasswordHashData>
          reused_protected_password_hash,
      size_t match_domain_count);

  void CheckStoresForReuse(const std::u16string& input);

  // A client to handle password reuse detection logic.
  raw_ptr<PasswordReuseDetectionManagerClient> client_;
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

  base::WeakPtrFactory<PasswordReuseDetectionManager> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_REUSE_DETECTION_MANAGER_H_
