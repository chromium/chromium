// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"

#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

// Maximum time since the last password check while the result is considered
// up to date.
constexpr auto kMaxTimeSinceLastCheck = base::TimeDelta::FromMinutes(30);

PostSaveCompromisedHelper::PostSaveCompromisedHelper(
    base::span<const CompromisedCredentials> compromised,
    const base::string16& current_username) {
  for (const CompromisedCredentials& credential : compromised) {
    if (credential.username == current_username)
      current_leak_ = credential;
  }
}

PostSaveCompromisedHelper::~PostSaveCompromisedHelper() = default;

void PostSaveCompromisedHelper::AnalyzeLeakedCredentials(
    PasswordStore* profile_store,
    PasswordStore* account_store,
    PrefService* prefs,
    BubbleCallback callback) {
  DCHECK(profile_store);
  DCHECK(prefs);
  callback_ = std::move(callback);
  prefs_ = prefs;
  compromised_credentials_reader_ =
      std::make_unique<CompromisedCredentialsReader>(profile_store,
                                                     account_store);
  // Unretained(this) is safe here since `this` outlives
  // `compromised_credentials_reader_`.
  compromised_credentials_reader_->GetAllCompromisedCredentials(
      base::BindOnce(&PostSaveCompromisedHelper::OnGetAllCompromisedCredentials,
                     base::Unretained(this)));
}

void PostSaveCompromisedHelper::OnGetAllCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  const bool compromised_password_changed =
      current_leak_ && !base::Contains(compromised_credentials, *current_leak_);
  bubble_type_ = BubbleType::kNoBubble;
  if (compromised_credentials.empty()) {
    if (compromised_password_changed) {
      // Obtain the timestamp of the last completed check. This is 0.0 in case
      // the check never completely ran before.
      const double last_check_completed =
          prefs_->GetDouble(prefs::kLastTimePasswordCheckCompleted);
      if (last_check_completed &&
          base::Time::Now() - base::Time::FromDoubleT(last_check_completed) <
              kMaxTimeSinceLastCheck) {
        bubble_type_ = BubbleType::kPasswordUpdatedSafeState;
      }
    }
  } else {
    bubble_type_ = compromised_password_changed
                       ? BubbleType::kPasswordUpdatedWithMoreToFix
                       : BubbleType::kUnsafeState;
  }
  compromised_count_ = compromised_credentials.size();
  std::move(callback_).Run(bubble_type_, compromised_count_);
}

}  // namespace password_manager
