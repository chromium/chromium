// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detection_manager.h"

#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/safe_browsing/buildflags.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using base::Time;
using base::TimeDelta;

namespace password_manager {

namespace {
constexpr size_t kMaxNumberOfCharactersToStore = 45;
constexpr TimeDelta kMaxInactivityTime = TimeDelta::FromSeconds(10);
}  // namespace

PasswordReuseDetectionManager::PasswordReuseDetectionManager(
    PasswordManagerClient* client)
    : client_(client), clock_(base::DefaultClock::GetInstance()) {
  DCHECK(client_);
}

PasswordReuseDetectionManager::~PasswordReuseDetectionManager() = default;

void PasswordReuseDetectionManager::DidNavigateMainFrame(
    const GURL& main_frame_url) {
  if (main_frame_url.host() == main_frame_url_.host())
    return;

  main_frame_url_ = main_frame_url;
  input_characters_.clear();
  reuse_on_this_page_was_found_ = false;
}

void PasswordReuseDetectionManager::OnKeyPressedCommitted(
    const base::string16& text) {
  OnKeyPressed(text, /*is_committed*/ true);
}

#if defined(OS_ANDROID)
void PasswordReuseDetectionManager::OnKeyPressedUncommitted(
    const base::string16& text) {
  OnKeyPressed(text, /*is_committed*/ false);
}
#endif

void PasswordReuseDetectionManager::OnKeyPressed(const base::string16& text,
                                                 const bool is_committed) {
  // Do not check reuse if it was already found on this page.
  if (reuse_on_this_page_was_found_)
    return;

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

  if (is_committed)
    input_characters_ += text;

  if (input_characters_.size() > kMaxNumberOfCharactersToStore) {
    input_characters_.erase(
        0, input_characters_.size() - kMaxNumberOfCharactersToStore);
  }

  const base::string16 text_to_check =
      is_committed ? input_characters_ : input_characters_ + text;

  CheckStoresForReuse(text_to_check);
}

void PasswordReuseDetectionManager::OnPaste(const base::string16 text) {
  // Do not check reuse if it was already found on this page.
  if (reuse_on_this_page_was_found_)
    return;
  base::string16 input = std::move(text);
  if (input.size() > kMaxNumberOfCharactersToStore)
    input = input.substr(input.size() - kMaxNumberOfCharactersToStore);

  CheckStoresForReuse(input);
}

void PasswordReuseDetectionManager::OnReuseCheckDone(
    bool is_reuse_found,
    size_t password_length,
    base::Optional<PasswordHashData> reused_protected_password_hash,
    const std::vector<MatchingReusedCredential>& matching_reused_credentials,
    int saved_passwords) {
  // Cache the results.
  all_matching_reused_credentials_.insert(matching_reused_credentials.begin(),
                                          matching_reused_credentials.end());
  reuse_on_this_page_was_found_ |= is_reuse_found;

  // If we're still waiting for more results, nothing to be done yet.
  if (--wait_counter_ > 0)
    return;

  // If no reuse was found, we're done.
  if (!reuse_on_this_page_was_found_) {
    all_matching_reused_credentials_.clear();
    return;
  }

  metrics_util::PasswordType reused_password_type = GetReusedPasswordType(
      reused_protected_password_hash, all_matching_reused_credentials_.size());

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    std::vector<std::string> domains_to_log;
    domains_to_log.reserve(all_matching_reused_credentials_.size());
    for (const MatchingReusedCredential& credential :
         all_matching_reused_credentials_) {
      domains_to_log.push_back(credential.signon_realm);
    }
    switch (reused_password_type) {
      case metrics_util::PasswordType::PRIMARY_ACCOUNT_PASSWORD:
        domains_to_log.push_back("CHROME SYNC PASSWORD");
        break;
      case metrics_util::PasswordType::OTHER_GAIA_PASSWORD:
        domains_to_log.push_back("OTHER GAIA PASSWORD");
        break;
      case metrics_util::PasswordType::ENTERPRISE_PASSWORD:
        domains_to_log.push_back("ENTERPRISE PASSWORD");
        break;
      case metrics_util::PasswordType::SAVED_PASSWORD:
        domains_to_log.push_back("SAVED PASSWORD");
        break;
      default:
        break;
    }

    for (const auto& domain : domains_to_log) {
      logger.LogString(BrowserSavePasswordProgressLogger::STRING_REUSE_FOUND,
                       domain);
    }
  }

  // PasswordManager could be nullptr in tests.
  bool password_field_detected =
      client_->GetPasswordManager()
          ? client_->GetPasswordManager()->IsPasswordFieldDetectedOnPage()
          : false;

  metrics_util::LogPasswordReuse(password_length, saved_passwords,
                                 all_matching_reused_credentials_.size(),
                                 password_field_detected, reused_password_type);
#if defined(PASSWORD_REUSE_WARNING_ENABLED)
  if (reused_password_type ==
      metrics_util::PasswordType::PRIMARY_ACCOUNT_PASSWORD)
    client_->LogPasswordReuseDetectedEvent();
#endif

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  std::string username = reused_protected_password_hash.has_value()
                             ? reused_protected_password_hash->username
                             : "";

  client_->CheckProtectedPasswordEntry(
      reused_password_type, username,
      std::move(all_matching_reused_credentials_).extract(),
      password_field_detected);
#endif
  all_matching_reused_credentials_.clear();
}

void PasswordReuseDetectionManager::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

metrics_util::PasswordType PasswordReuseDetectionManager::GetReusedPasswordType(
    base::Optional<PasswordHashData> reused_protected_password_hash,
    size_t matching_domain_count) {
  if (!reused_protected_password_hash.has_value()) {
    DCHECK_GT(matching_domain_count, 0u);
    return metrics_util::PasswordType::SAVED_PASSWORD;
  }

  if (!reused_protected_password_hash->is_gaia_password) {
    return metrics_util::PasswordType::ENTERPRISE_PASSWORD;
  } else if (client_->GetStoreResultFilter()->IsSyncAccountEmail(
                 reused_protected_password_hash->username)) {
    return metrics_util::PasswordType::PRIMARY_ACCOUNT_PASSWORD;
  } else {
    return metrics_util::PasswordType::OTHER_GAIA_PASSWORD;
  }
}

void PasswordReuseDetectionManager::CheckStoresForReuse(
    const base::string16& input) {
  PasswordStore* profile_store = client_->GetProfilePasswordStore();
  if (profile_store) {
    ++wait_counter_;
    profile_store->CheckReuse(input, main_frame_url_.GetOrigin().spec(), this);
  }

  PasswordStore* account_store = client_->GetAccountPasswordStore();
  if (account_store) {
    ++wait_counter_;
    account_store->CheckReuse(input, main_frame_url_.GetOrigin().spec(), this);
  }
}

}  // namespace password_manager
