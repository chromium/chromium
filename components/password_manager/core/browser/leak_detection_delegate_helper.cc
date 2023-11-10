// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"

namespace password_manager {

LeakDetectionDelegateHelper::LeakDetectionDelegateHelper(
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store,
    LeakTypeReply callback)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)),
      callback_(std::move(callback)) {
  DCHECK(profile_store_);
  // `account_store_` may be null.
}

LeakDetectionDelegateHelper::~LeakDetectionDelegateHelper() = default;

void LeakDetectionDelegateHelper::ProcessLeakedPassword(
    GURL url,
    std::u16string username,
    std::u16string password) {
  url_ = std::move(url);
  username_ = std::move(username);
  password_ = std::move(password);

  int wait_counter = 1 + (account_store_ ? 1 : 0);
  barrier_closure_ = base::BarrierClosure(
      wait_counter, base::BindOnce(&LeakDetectionDelegateHelper::ProcessResults,
                                   base::Unretained(this)));

  profile_store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());

  if (account_store_) {
    account_store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

void LeakDetectionDelegateHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Store the results.
  base::ranges::move(results, std::back_inserter(partial_results_));

  barrier_closure_.Run();
}

void LeakDetectionDelegateHelper::ProcessResults() {
  std::u16string canonicalized_username = CanonicalizeUsername(username_);
  std::vector<GURL> all_urls_with_leaked_credentials;
  PasswordForm::Store in_stores = PasswordForm::Store::kNotSet;

  // Returns true if the urls are identical or one is a PSL match of the other.
  auto are_urls_equivalent = [](const GURL& url1, const GURL& url2) -> bool {
    return url1 == url2 || IsPublicSuffixDomainMatch(url1.spec(), url2.spec());
  };

  for (const auto& form : partial_results_) {
    if (CanonicalizeUsername(form->username_value) == canonicalized_username &&
        form->password_value == password_) {
      PasswordStoreInterface& store =
          form->IsUsingAccountStore() ? *account_store_ : *profile_store_;
      // crbug.com/1381203: It's very important not to touch already leaked
      // passwords. It overwrites the date and leads to performance problems as
      // called in the loop.
      if (!form->password_issues.contains(InsecureType::kLeaked)) {
        PasswordForm form_to_update = *form.get();
        form_to_update.password_issues.insert_or_assign(
            InsecureType::kLeaked,
            InsecurityMetadata(base::Time::Now(), IsMuted(false),
                               TriggerBackendNotification(false)));
        store.UpdateLogin(form_to_update);
      }
      all_urls_with_leaked_credentials.push_back(form->url);

      if (are_urls_equivalent(form->url, url_)) {
        in_stores = in_stores | form->in_store;
      }
    }
  }

  // Check if the password is reused on a different origin, or on the same
  // origin with a different username.
  IsReused is_reused(base::ranges::any_of(
      partial_results_, [this, are_urls_equivalent](const auto& form) {
        return form->password_value == password_ &&
               (!are_urls_equivalent(form->url, url_) ||
                form->username_value != username_);
      }));

  std::move(callback_).Run(in_stores, is_reused, std::move(url_),
                           std::move(username_),
                           std::move(all_urls_with_leaked_credentials));
}

}  // namespace password_manager
