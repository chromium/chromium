// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "url/gurl.h"

namespace {
using password_manager::metrics_util::IsPasswordChanged;
using password_manager::metrics_util::IsPasswordNoteChanged;
using password_manager::metrics_util::IsUsernameChanged;
using PasswordNote = password_manager::PasswordNote;
using Store = password_manager::PasswordForm::Store;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;

bool IsUsernameAlreadyUsed(SavedPasswordsView all_forms,
                           SavedPasswordsView forms_to_check,
                           const std::u16string& new_username) {
  // In case the username changed, make sure that there exists no other
  // credential with the same signon_realm and username in the same store.
  auto has_conflicting_username = [&forms_to_check,
                                   &new_username](const auto& form) {
    return new_username == form.username_value &&
           base::ranges::any_of(forms_to_check, [&form](const auto& old_form) {
             return form.signon_realm == old_form.signon_realm &&
                    form.IsUsingAccountStore() ==
                        old_form.IsUsingAccountStore();
           });
  };
  return base::ranges::any_of(all_forms, has_conflicting_username);
}

// Returns trues if there is at least one password store that contains both
// passwords.
constexpr bool ShareSameStore(const password_manager::PasswordForm& lhs,
                              const password_manager::PasswordForm& rhs) {
  return (lhs.in_store & rhs.in_store) != Store::kNotSet;
}
}  // namespace

namespace password_manager {

SavedPasswordsPresenter::SavedPasswordsPresenter(
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)) {
  DCHECK(profile_store_);
  profile_store_->AddObserver(this);
  if (account_store_)
    account_store_->AddObserver(this);
}

SavedPasswordsPresenter::~SavedPasswordsPresenter() {
  if (account_store_)
    account_store_->RemoveObserver(this);
  profile_store_->RemoveObserver(this);
}

void SavedPasswordsPresenter::Init() {
  profile_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
  if (account_store_) {
    account_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
        weak_ptr_factory_.GetWeakPtr());
  }
}

void SavedPasswordsPresenter::RemovePassword(const PasswordForm& form) {
  std::string current_form_key = CreateSortKey(form, IgnoreStore(true));
  const auto range = sort_key_to_password_forms_.equal_range(current_form_key);

  std::for_each(range.first, range.second, [&](const auto& pair) {
    const auto& current_form = pair.second;
    // Make sure |form| and |current_form| share the same store.
    if (ShareSameStore(form, current_form)) {
      // |current_form| is unchanged result obtained from
      // 'OnGetPasswordStoreResultsFrom'. So it can be present only in one store
      // at a time..
      GetStoreFor(current_form).RemoveLogin(current_form);
    }
  });
}

bool SavedPasswordsPresenter::AddPassword(const PasswordForm& form) {
  if (!password_manager_util::IsValidPasswordURL(form.url))
    return false;
  if (form.password_value.empty())
    return false;

  auto have_equal_username_and_realm = [&form](const PasswordForm& entry) {
    return form.signon_realm == entry.signon_realm &&
           form.username_value == entry.username_value;
  };
  if (base::ranges::any_of(passwords_, have_equal_username_and_realm))
    return false;

  // Try to unblocklist in both stores anyway because if credentials don't
  // exist, the unblocklist operation is no-op.
  auto form_digest = PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                        form.signon_realm, form.url);
  profile_store_->Unblocklist(form_digest);
  if (account_store_)
    account_store_->Unblocklist(form_digest);

  GetStoreFor(form).AddLogin(form);
  metrics_util::LogUserInteractionsWhenAddingCredentialFromSettings(
      metrics_util::AddCredentialFromSettingsUserInteractions::
          kCredentialAdded);
  return true;
}

bool SavedPasswordsPresenter::EditPassword(const PasswordForm& form,
                                           std::u16string new_password) {
  auto is_equal = [&form](const PasswordForm& form_to_check) {
    return ArePasswordFormUniqueKeysEqual(form, form_to_check);
  };
  auto found = base::ranges::find_if(passwords_, is_equal);
  if (found == passwords_.end())
    return false;

  found->password_value = std::move(new_password);
  found->date_password_modified = base::Time::Now();
  found->password_issues.clear();
  PasswordStoreInterface& store =
      form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
  store.UpdateLogin(*found);
  NotifyEdited(*found);
  return true;
}

bool SavedPasswordsPresenter::EditSavedPasswords(
    const PasswordForm& form,
    const std::u16string& new_username,
    const std::u16string& new_password) {
  // TODO(crbug.com/1184691): Change desktop settings and maybe iOS to use this
  // presenter for updating the duplicates.
  std::vector<PasswordForm> forms_to_change;

  std::string current_form_key = CreateSortKey(form, IgnoreStore(true));
  const auto range = sort_key_to_password_forms_.equal_range(current_form_key);

  base::ranges::transform(range.first, range.second,
                          std::back_inserter(forms_to_change),
                          [](const auto& pair) { return pair.second; });
  return EditSavedPasswords(forms_to_change, new_username, new_password);
}

bool SavedPasswordsPresenter::EditSavedPasswords(
    const SavedPasswordsView forms,
    const std::u16string& new_username,
    const std::u16string& new_password,
    const std::u16string& new_note) {
  if (forms.empty())
    return false;
  IsUsernameChanged username_changed(new_username != forms[0].username_value);
  IsPasswordChanged password_changed(new_password != forms[0].password_value);
  IsPasswordNoteChanged note_changed =
      IsPasswordNoteChanged(forms[0].note.value != new_note);

  if (new_password.empty())
    return false;
  if (username_changed &&
      IsUsernameAlreadyUsed(passwords_, forms, new_username))
    return false;

  // An updated username implies a change in the primary key, thus we need to
  // make sure to call the right API. Update every entry in the equivalence
  // class.
  if (username_changed || password_changed || note_changed) {
    for (const auto& old_form : forms) {
      PasswordStoreInterface& store = GetStoreFor(old_form);
      PasswordForm new_form = old_form;

      if (password_changed) {
        new_form.password_value = new_password;
        new_form.date_password_modified = base::Time::Now();
        new_form.password_issues.clear();
      }

      if (note_changed) {
        // if the old note is empty, the note is just created.
        if (old_form.note.value.empty()) {
          new_form.note.date_created = base::Time::Now();
        }
        new_form.note.value = new_note;
      }

      if (username_changed) {
        new_form.username_value = new_username;
        // Phished and leaked issues are no longer relevant on username change.
        // Weak and reused issues are still relevant.
        new_form.password_issues.erase(InsecureType::kPhished);
        new_form.password_issues.erase(InsecureType::kLeaked);
        // Changing username requires deleting old form and adding new one. So
        // the different API should be called.
        store.UpdateLoginWithPrimaryKey(new_form, old_form);
      } else {
        store.UpdateLogin(new_form);
      }
      NotifyEdited(new_form);
    }
  }

  password_manager::metrics_util::LogPasswordEditResult(username_changed,
                                                        password_changed);
  return true;
}

SavedPasswordsPresenter::SavedPasswordsView
SavedPasswordsPresenter::GetSavedPasswords() const {
  return passwords_;
}

std::vector<PasswordForm> SavedPasswordsPresenter::GetUniquePasswordForms()
    const {
  std::vector<PasswordForm> forms;

  auto it = sort_key_to_password_forms_.begin();
  std::string current_key;

  while (it != sort_key_to_password_forms_.end()) {
    if (current_key != it->first) {
      current_key = it->first;
      forms.push_back(it->second);
    } else {
      forms.back().in_store = forms.back().in_store | it->second.in_store;
    }
    ++it;
  }

  return forms;
}

std::vector<std::u16string> SavedPasswordsPresenter::GetUsernamesForRealm(
    const std::string& signon_realm,
    bool is_using_account_store) {
  std::vector<std::u16string> usernames;
  for (const auto& form : passwords_) {
    if (form.signon_realm == signon_realm &&
        form.IsUsingAccountStore() == is_using_account_store) {
      usernames.push_back(form.username_value);
    }
  }
  return usernames;
}

void SavedPasswordsPresenter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SavedPasswordsPresenter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SavedPasswordsPresenter::NotifyEdited(const PasswordForm& password) {
  for (auto& observer : observers_)
    observer.OnEdited(password);
}

void SavedPasswordsPresenter::NotifySavedPasswordsChanged() {
  for (auto& observer : observers_)
    observer.OnSavedPasswordsChanged(passwords_);
}

void SavedPasswordsPresenter::OnLoginsChanged(
    PasswordStoreInterface* store,
    const PasswordStoreChangeList& changes) {
  store->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
}

void SavedPasswordsPresenter::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  store->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
}

void SavedPasswordsPresenter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // This class overrides OnGetPasswordStoreResultsFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void SavedPasswordsPresenter::OnGetPasswordStoreResultsFrom(
    PasswordStoreInterface* store,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Profile store passwords are always stored first in `passwords_`.
  auto account_passwords_it = base::ranges::partition_point(
      passwords_,
      [](auto& password) { return !password.IsUsingAccountStore(); });
  if (store == profile_store_) {
    // Old profile store passwords are in front. Create a temporary buffer for
    // the new passwords and replace existing passwords.
    std::vector<PasswordForm> new_passwords;
    new_passwords.reserve(results.size() + passwords_.end() -
                          account_passwords_it);
    auto new_passwords_back_inserter = std::back_inserter(new_passwords);
    base::ranges::transform(results, new_passwords_back_inserter,
                            [](auto& result) { return std::move(*result); });
    std::move(account_passwords_it, passwords_.end(),
              new_passwords_back_inserter);
    passwords_ = std::move(new_passwords);
  } else {
    // Need to replace existing account passwords at the end. Can re-use
    // existing `passwords_` vector.
    passwords_.erase(account_passwords_it, passwords_.end());
    if (passwords_.capacity() < passwords_.size() + results.size())
      passwords_.reserve(passwords_.size() + results.size());
    base::ranges::transform(results, std::back_inserter(passwords_),
                            [](auto& result) { return std::move(*result); });
  }

  sort_key_to_password_forms_.clear();
  base::ranges::for_each(passwords_, [&](const auto& result) {
    sort_key_to_password_forms_.insert(
        std::make_pair(CreateSortKey(result, IgnoreStore(true)), result));
  });

  // Remove blocked or federated credentials.
  base::EraseIf(passwords_, [](const auto& form) {
    return form.blocked_by_user || form.IsFederatedCredential();
  });

  NotifySavedPasswordsChanged();
}

PasswordStoreInterface& SavedPasswordsPresenter::GetStoreFor(
    const PasswordForm& form) {
  DCHECK_NE(form.IsUsingAccountStore(), form.IsUsingProfileStore());
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

}  // namespace password_manager
