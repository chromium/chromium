// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager.h"

#include <optional>

#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager_client.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using base::Time;

namespace safe_browsing {

namespace {
// The maximum number of characters to store in the keystrokes buffer.
constexpr size_t kMaxNumberOfCharactersToStore = 45;
// Clears the keystrokes buffer if last keystoke was more than 10s ago.
constexpr base::TimeDelta kMaxInactivityTime = base::Seconds(10);
}  // namespace

PasswordReuseDetectionManager::PasswordReuseDetectionManager(
    PasswordReuseDetectionManagerClient* client)
    : client_(client), clock_(base::DefaultClock::GetInstance()) {
  CHECK(client_);
}

PasswordReuseDetectionManager::~PasswordReuseDetectionManager() = default;

void PasswordReuseDetectionManager::DidNavigateMainFrame(
    const GURL& main_frame_url) {
  if (main_frame_url.host() == main_frame_url_.host()) {
    return;
  }

  main_frame_url_ = main_frame_url;
  input_characters_.clear();
  reuse_on_this_page_was_found_ = false;
}

void PasswordReuseDetectionManager::OnKeyPressedCommitted(
    const std::u16string& text) {
  OnKeyPressed(text, /*is_committed=*/true);
}

#if BUILDFLAG(IS_ANDROID)
void PasswordReuseDetectionManager::OnKeyPressedUncommitted(
    const std::u16string& text) {
  OnKeyPressed(text, /*is_committed=*/false);
}
#endif

void PasswordReuseDetectionManager::OnKeyPressed(const std::u16string& text,
                                                 bool is_committed) {
  // Do not check reuse if it was already found on this page.
  if (reuse_on_this_page_was_found_) {
    return;
  }

  // Clear the buffer if last keystoke was more than kMaxInactivityTime ago.
  Time now = clock_->Now();
  if (!last_keystroke_time_.is_null() &&
      (now - last_keystroke_time_) >= kMaxInactivityTime) {
    input_characters_.clear();
  }
  last_keystroke_time_ = now;

  // Clear the buffer and return when enter is pressed.
  if (text.size() == 1 && text[0] == ui::VKEY_RETURN) {
    input_characters_.clear();
    return;
  }

  if (is_committed) {
    input_characters_ += text;
  }

  if (input_characters_.size() > kMaxNumberOfCharactersToStore) {
    input_characters_.erase(
        0, input_characters_.size() - kMaxNumberOfCharactersToStore);
  }

  const std::u16string text_to_check =
      is_committed ? input_characters_ : input_characters_ + text;

  CheckStoresForReuse(text_to_check);
}

void PasswordReuseDetectionManager::OnPaste(std::u16string text) {
  // Do not check reuse if it was already found on this page.
  if (reuse_on_this_page_was_found_) {
    return;
  }
  if (text.size() > kMaxNumberOfCharactersToStore) {
    text = text.substr(text.size() - kMaxNumberOfCharactersToStore);
  }

  CheckStoresForReuse(text);
}

void PasswordReuseDetectionManager::OnReuseCheckDone(
    bool is_reuse_found,
    size_t password_length,
    std::optional<password_manager::PasswordHashData>
        reused_protected_password_hash,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    int saved_passwords,
    const std::string& domain,
    uint64_t reused_password_hash) {
  reuse_on_this_page_was_found_ |= is_reuse_found;

  // If no reuse was found, we're done.
  if (!reuse_on_this_page_was_found_) {
    return;
  }

  password_manager::metrics_util::PasswordType reused_password_type =
      GetReusedPasswordType(reused_protected_password_hash,
                            matching_reused_credentials.size());
  autofill::LogManager* log_manager = client_->GetLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(log_manager);
    std::vector<std::string> domains_to_log;
    domains_to_log.reserve(matching_reused_credentials.size());
    for (const password_manager::MatchingReusedCredential& credential :
         matching_reused_credentials) {
      domains_to_log.push_back(credential.signon_realm);
    }
    switch (reused_password_type) {
      case password_manager::metrics_util::PasswordType::
          PRIMARY_ACCOUNT_PASSWORD:
        domains_to_log.push_back("CHROME SYNC PASSWORD");
        break;
      case password_manager::metrics_util::PasswordType::OTHER_GAIA_PASSWORD:
        domains_to_log.push_back("OTHER GAIA PASSWORD");
        break;
      case password_manager::metrics_util::PasswordType::ENTERPRISE_PASSWORD:
        domains_to_log.push_back("ENTERPRISE PASSWORD");
        break;
      case password_manager::metrics_util::PasswordType::SAVED_PASSWORD:
        domains_to_log.push_back("SAVED PASSWORD");
        break;
      default:
        break;
    }

    for (const auto& domain_to_log : domains_to_log) {
      logger.LogString(password_manager::BrowserSavePasswordProgressLogger::
                           STRING_REUSE_FOUND,
                       domain_to_log);
    }
  }

  bool password_field_detected = client_->IsPasswordFieldDetectedOnPage();

  password_manager::metrics_util::LogPasswordReuse(
      saved_passwords, matching_reused_credentials.size(),
      password_field_detected, reused_password_type);
  if (reused_password_type ==
      password_manager::metrics_util::PasswordType::PRIMARY_ACCOUNT_PASSWORD) {
    client_->MaybeLogPasswordReuseDetectedEvent();
  }

  std::string username = reused_protected_password_hash.has_value()
                             ? reused_protected_password_hash->username
                             : "";

  client_->CheckProtectedPasswordEntry(
      reused_password_type, username, matching_reused_credentials,
      password_field_detected, reused_password_hash, domain);
}

base::WeakPtr<password_manager::PasswordReuseDetectorConsumer>
PasswordReuseDetectionManager::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordReuseDetectionManager::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

password_manager::metrics_util::PasswordType
PasswordReuseDetectionManager::GetReusedPasswordType(
    std::optional<password_manager::PasswordHashData>
        reused_protected_password_hash,
    size_t matching_domain_count) {
  if (!reused_protected_password_hash.has_value()) {
    DCHECK_GT(matching_domain_count, 0u);
    return password_manager::metrics_util::PasswordType::SAVED_PASSWORD;
  }

  password_manager::metrics_util::PasswordType reused_password_type;
  if (!reused_protected_password_hash->is_gaia_password) {
    reused_password_type =
        password_manager::metrics_util::PasswordType::ENTERPRISE_PASSWORD;
  } else if (client_->IsHistorySyncAccountEmail(
                 reused_protected_password_hash->username)) {
    reused_password_type =
        password_manager::metrics_util::PasswordType::PRIMARY_ACCOUNT_PASSWORD;
  } else {
    reused_password_type =
        password_manager::metrics_util::PasswordType::OTHER_GAIA_PASSWORD;
  }
  return reused_password_type;
}

void PasswordReuseDetectionManager::CheckStoresForReuse(
    const std::u16string& input) {
  password_manager::PasswordReuseManager* reuse_manager =
      client_->GetPasswordReuseManager();
  if (reuse_manager) {
    reuse_manager->CheckReuse(
        input, main_frame_url_.DeprecatedGetOriginAsURL().spec(), this);
  }
}

}  // namespace safe_browsing
