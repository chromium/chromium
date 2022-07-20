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
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/password_undo_helper.h"
#include "url/gurl.h"

namespace {
using password_manager::metrics_util::IsPasswordChanged;
using password_manager::metrics_util::IsPasswordNoteChanged;
using password_manager::metrics_util::IsUsernameChanged;
using password_manager::metrics_util::PasswordNoteAction;
using PasswordNote = password_manager::PasswordNote;
using Store = password_manager::PasswordForm::Store;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;
using EditResult = password_manager::SavedPasswordsPresenter::EditResult;

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

password_manager::PasswordForm GenerateFormFromCredential(
    password_manager::CredentialUIEntry credential) {
  password_manager::PasswordForm form;
  form.url = credential.url;
  form.signon_realm = credential.signon_realm;
  form.username_value = credential.username;
  form.password_value = credential.password;
  if (!credential.note.value.empty())
    form.notes = {credential.note};

  DCHECK(!credential.stored_in.empty());
  form.in_store = *credential.stored_in.begin();
  return form;
}

// Check if notes was modified for a specified |form| with |new_note|.
IsPasswordNoteChanged IsNoteChanged(const password_manager::PasswordForm& form,
                                    const PasswordNote& new_note) {
  const auto& old_note_itr = base::ranges::find_if(
      form.notes, &std::u16string::empty, &PasswordNote::unique_display_name);
  bool old_note_exists = old_note_itr != form.notes.end();
  return IsPasswordNoteChanged(
      (old_note_exists && old_note_itr->value != new_note.value) ||
      (!old_note_exists && !new_note.value.empty()));
}

PasswordNoteAction UpdateNoteInPasswordForm(
    password_manager::PasswordForm& form,
    const PasswordNote& new_note) {
  const auto& note_itr = base::ranges::find_if(
      form.notes, &std::u16string::empty, &PasswordNote::unique_display_name);
  // if the old note doesn't exist, the note is just created.
  if (note_itr == form.notes.end()) {
    form.notes.push_back(new_note);
    return PasswordNoteAction::kNoteAddedInEditDialog;
  }
  // Note existed, but it was empty.
  if (note_itr->value.empty()) {
    note_itr->value = new_note.value;
    note_itr->date_created = base::Time::Now();
    return PasswordNoteAction::kNoteAddedInEditDialog;
  }
  note_itr->value = new_note.value;
  return new_note.value.empty() ? PasswordNoteAction::kNoteRemovedInEditDialog
                                : PasswordNoteAction::kNoteEditedInEditDialog;
}

}  // namespace

namespace password_manager {

SavedPasswordsPresenter::SavedPasswordsPresenter(
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)),
      undo_helper_(std::make_unique<PasswordUndoHelper>(profile_store_.get(),
                                                        account_store_.get())) {
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
  RemoveCredential(CredentialUIEntry(form));
}

bool SavedPasswordsPresenter::RemoveCredential(
    const CredentialUIEntry& credential) {
  const auto range =
      sort_key_to_password_forms_.equal_range(CreateSortKey(credential));
  bool removed = false;
  undo_helper_->StartGroupingActions();
  std::for_each(range.first, range.second, [&](const auto& pair) {
    const auto& current_form = pair.second;
    // Make sure |credential| and |current_form| share the same store.
    if (credential.stored_in.contains(current_form.in_store)) {
      // |current_form| is unchanged result obtained from
      // 'OnGetPasswordStoreResultsFrom'. So it can be present only in one store
      // at a time..
      GetStoreFor(current_form).RemoveLogin(current_form);
      undo_helper_->PasswordRemoved(current_form);
      removed = true;
    }
  });
  undo_helper_->EndGroupingActions();
  return removed;
}

void SavedPasswordsPresenter::UndoLastRemoval() {
  undo_helper_->Undo();
}

bool SavedPasswordsPresenter::AddCredential(
    const CredentialUIEntry& credential) {
  if (!password_manager_util::IsValidPasswordURL(credential.url))
    return false;
  if (credential.password.empty())
    return false;

  auto have_equal_username_and_realm =
      [&credential](const PasswordForm& entry) {
        return credential.signon_realm == entry.signon_realm &&
               credential.username == entry.username_value;
      };
  if (base::ranges::any_of(passwords_, have_equal_username_and_realm))
    return false;

  // Try to unblocklist in both stores anyway because if credentials don't
  // exist, the unblocklist operation is no-op.
  auto form_digest = PasswordFormDigest(
      PasswordForm::Scheme::kHtml, credential.signon_realm, credential.url);
  profile_store_->Unblocklist(form_digest);
  if (account_store_)
    account_store_->Unblocklist(form_digest);

  PasswordForm form = GenerateFormFromCredential(credential);
  form.type = password_manager::PasswordForm::Type::kManuallyAdded;
  form.date_created = base::Time::Now();
  form.date_password_modified = base::Time::Now();

  GetStoreFor(form).AddLogin(form);
  metrics_util::LogUserInteractionsWhenAddingCredentialFromSettings(
      metrics_util::AddCredentialFromSettingsUserInteractions::
          kCredentialAdded);
  if (!form.notes.empty() && form.notes[0].value.length() > 0) {
    metrics_util::LogPasswordNoteActionInSettings(
        PasswordNoteAction::kNoteAddedInAddDialog);
  }
  return true;
}

bool SavedPasswordsPresenter::EditPassword(const PasswordForm& form,
                                           std::u16string new_password) {
  CredentialUIEntry updated_credential(form);
  updated_credential.password = new_password;
  return EditSavedCredentials(CredentialUIEntry(form), updated_credential) ==
         EditResult::kSuccess;
}

bool SavedPasswordsPresenter::EditSavedPasswords(
    const PasswordForm& form,
    const std::u16string& new_username,
    const std::u16string& new_password) {
  // TODO(crbug.com/1184691): Change desktop settings and maybe iOS to use this
  // presenter for updating the duplicates.
  CredentialUIEntry updated_credential(form);
  updated_credential.password = new_password;
  updated_credential.username = new_username;
  return EditSavedCredentials(CredentialUIEntry(form), updated_credential) ==
         EditResult::kSuccess;
}

SavedPasswordsPresenter::EditResult
SavedPasswordsPresenter::EditSavedCredentials(
    const CredentialUIEntry& original_credential,
    const CredentialUIEntry& updated_credential) {
  std::vector<PasswordForm> forms_to_change =
      GetCorrespondingPasswordForms(original_credential);
  if (forms_to_change.empty())
    return EditResult::kNotFound;

  IsUsernameChanged username_changed(updated_credential.username !=
                                     original_credential.username);
  IsPasswordChanged password_changed(updated_credential.password !=
                                     original_credential.password);
  IsPasswordNoteChanged note_changed =
      IsNoteChanged(forms_to_change[0], updated_credential.note);

  bool issues_changed =
      updated_credential.password_issues != forms_to_change[0].password_issues;

  // Password can't be empty.
  if (updated_credential.password.empty())
    return EditResult::kEmptyPassword;

  // Username can't be changed to the existing one.
  if (username_changed && IsUsernameAlreadyUsed(passwords_, forms_to_change,
                                                updated_credential.username)) {
    return EditResult::kAlreadyExisits;
  }

  // Nothing changed.
  if (!username_changed && !password_changed && !note_changed &&
      !issues_changed) {
    password_manager::metrics_util::LogPasswordEditResult(username_changed,
                                                          password_changed);
    return EditResult::kNothingChanged;
  }

  for (const auto& old_form : forms_to_change) {
    PasswordStoreInterface& store = GetStoreFor(old_form);
    PasswordForm new_form = old_form;

    if (issues_changed) {
      new_form.password_issues = updated_credential.password_issues;
    }

    if (password_changed) {
      new_form.password_value = updated_credential.password;
      new_form.date_password_modified = base::Time::Now();
      new_form.password_issues.clear();
    }

    if (note_changed) {
      PasswordNoteAction note_action =
          UpdateNoteInPasswordForm(new_form, updated_credential.note);
      metrics_util::LogPasswordNoteActionInSettings(note_action);
    }

    // An updated username implies a change in the primary key, thus we need
    // to make sure to call the right API.
    if (username_changed) {
      new_form.username_value = updated_credential.username;
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

  password_manager::metrics_util::LogPasswordEditResult(username_changed,
                                                        password_changed);
  return EditResult::kSuccess;
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

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetSavedCredentials()
    const {
  std::vector<PasswordForm> forms = GetUniquePasswordForms();
  std::vector<CredentialUIEntry> credentials;
  credentials.reserve(forms.size());
  base::ranges::transform(
      forms, std::back_inserter(credentials),
      [](const PasswordForm& form) { return CredentialUIEntry(form); });
  return credentials;
}

std::vector<PasswordForm>
SavedPasswordsPresenter::GetCorrespondingPasswordForms(
    const CredentialUIEntry& credential) const {
  const auto range =
      sort_key_to_password_forms_.equal_range(CreateSortKey(credential));
  std::vector<PasswordForm> forms;
  base::ranges::transform(range.first, range.second, std::back_inserter(forms),
                          [](const auto& pair) { return pair.second; });
  return forms;
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
  bool is_account_store = store == account_store_.get();

  // Remove cached credentials for current store.
  base::EraseIf(sort_key_to_password_forms_,
                [&is_account_store](const auto& pair) {
                  return pair.second.IsUsingAccountStore() == is_account_store;
                });

  // Move |results| into |sort_key_to_password_forms_|.
  base::ranges::for_each(results, [&](const auto& result) {
    PasswordForm form = std::move(*result);
    sort_key_to_password_forms_.insert(std::make_pair(
        CreateSortKey(form, IgnoreStore(true)), std::move(form)));
  });

  // Update |passwords_|.
  passwords_.clear();
  base::ranges::for_each(sort_key_to_password_forms_, [&](const auto& pair) {
    PasswordForm form = pair.second;
    if (!form.blocked_by_user && !form.IsFederatedCredential())
      passwords_.push_back(std::move(form));
  });

  NotifySavedPasswordsChanged();
}

PasswordStoreInterface& SavedPasswordsPresenter::GetStoreFor(
    const PasswordForm& form) {
  DCHECK_NE(form.IsUsingAccountStore(), form.IsUsingProfileStore());
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

}  // namespace password_manager
