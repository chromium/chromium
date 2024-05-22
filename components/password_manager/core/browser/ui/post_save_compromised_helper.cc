// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

// Maximum time since the last password check while the result is considered
// up to date.
constexpr auto kMaxTimeSinceLastCheck = base::Minutes(30);

PostSaveCompromisedHelper::PostSaveCompromisedHelper(
    const base::span<const PasswordForm> compromised,
    const std::u16string& current_username) {
  for (const PasswordForm& credential : compromised) {
    if (credential.username_value == current_username) {
      current_leak_ = credential;
    }
  }
}

PostSaveCompromisedHelper::~PostSaveCompromisedHelper() = default;

void PostSaveCompromisedHelper::AnalyzeLeakedCredentials(
    PasswordStoreInterface* profile_store,
    PasswordStoreInterface* account_store,
    PrefService* prefs,
    BubbleCallback callback) {
  DCHECK(profile_store);
  DCHECK(prefs);

  const double last_check_completed =
      prefs->GetDouble(prefs::kLastTimePasswordCheckCompleted);
  // If the check was never completed then |kLastTimePasswordCheckCompleted|
  // contains 0.
  if (!last_check_completed ||
      base::Time::Now() -
              base::Time::FromSecondsSinceUnixEpoch(last_check_completed) >=
          kMaxTimeSinceLastCheck) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), BubbleType::kNoBubble, 0));
    return;
  }

  callback_ = std::move(callback);

  int awaiting_callbacks = 1;
  if (account_store) {
    awaiting_callbacks++;
  }

  forms_received_ = base::BarrierClosure(
      awaiting_callbacks,
      base::BindOnce(
          &PostSaveCompromisedHelper::AnalyzeLeakedCredentialsInternal,
          base::Unretained(this)));

  profile_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  if (account_store) {
    account_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

void PostSaveCompromisedHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  base::ranges::move(results, std::back_inserter(passwords_));
  forms_received_.Run();
}

void PostSaveCompromisedHelper::AnalyzeLeakedCredentialsInternal() {
  bool compromised_password_changed = false;

  for (const auto& form : passwords_) {
    if (current_leak_ &&
        form->username_value == current_leak_->username_value &&
        form->signon_realm == current_leak_->signon_realm) {
      if (form->password_issues.empty()) {
        compromised_password_changed = true;
      }
    }

    if (base::ranges::any_of(form->password_issues, [](const auto& issue) {
          return !issue.second.is_muted;
        })) {
      compromised_count_++;
    }
  }
  if (compromised_password_changed) {
    bubble_type_ = compromised_count_ == 0
                       ? BubbleType::kPasswordUpdatedSafeState
                       : BubbleType::kPasswordUpdatedWithMoreToFix;
  } else {
    bubble_type_ = BubbleType::kNoBubble;
  }
  std::move(callback_).Run(bubble_type_, compromised_count_);
}

}  // namespace password_manager
