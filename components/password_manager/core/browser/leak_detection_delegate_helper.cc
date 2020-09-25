// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

LeakDetectionDelegateHelper::LeakDetectionDelegateHelper(
    scoped_refptr<PasswordStore> profile_store,
    scoped_refptr<PasswordStore> account_store,
    LeakTypeReply callback)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)),
      callback_(std::move(callback)) {
  DCHECK(profile_store_);
}

LeakDetectionDelegateHelper::~LeakDetectionDelegateHelper() = default;

void LeakDetectionDelegateHelper::ProcessLeakedPassword(
    GURL url,
    base::string16 username,
    base::string16 password) {
  url_ = std::move(url);
  username_ = std::move(username);
  password_ = std::move(password);

  ++wait_counter_;
  profile_store_->GetLoginsByPassword(password_, this);

  if (account_store_) {
    ++wait_counter_;
    account_store_->GetLoginsByPassword(password_, this);
  }
}

void LeakDetectionDelegateHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Store the results.
  base::ranges::move(results, std::back_inserter(partial_results_));

  // If we're still awaiting more results, nothing else to do.
  if (--wait_counter_ > 0)
    return;

  base::flat_set<std::string> distinct_origins;
  if (base::FeatureList::IsEnabled(features::kPasswordCheck)) {
    base::string16 canonicalized_username = CanonicalizeUsername(username_);
    for (const auto& form : partial_results_) {
      if (CanonicalizeUsername(form->username_value) ==
          canonicalized_username) {
        distinct_origins.insert(form->signon_realm);
        PasswordStore& store =
            form->IsUsingAccountStore() ? *account_store_ : *profile_store_;
        store.AddCompromisedCredentials(
            {form->signon_realm, form->username_value, base::Time::Now(),
             CompromiseType::kLeaked});
      }
    }
  }

  IsSaved is_saved(
      base::ranges::any_of(partial_results_, [this](const auto& form) {
        return form->url == url_ && form->username_value == username_;
      }));

  // Number of compromised origins that the user saved.
  CompromisedSitesCount saved_sites(distinct_origins.size());

  IsReused is_reused(partial_results_.size() > (is_saved ? 1 : 0));
  std::move(callback_).Run(is_saved, is_reused, std::move(url_),
                           std::move(username_), saved_sites);
}

}  // namespace password_manager
