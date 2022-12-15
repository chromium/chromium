// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/password_undo_helper.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/features.h"
#include "url/gurl.h"

namespace {
using password_manager::metrics_util::IsPasswordChanged;
using password_manager::metrics_util::IsPasswordNoteChanged;
using password_manager::metrics_util::IsUsernameChanged;
using password_manager::metrics_util::PasswordNoteAction;
using PasswordNote = password_manager::PasswordNote;
using Store = password_manager::PasswordForm::Store;
using EditResult = password_manager::SavedPasswordsPresenter::EditResult;

bool IsUsernameAlreadyUsed(
    password_manager::SavedPasswordsPresenter::DuplicatePasswordsMap
        key_to_forms,
    const std::vector<password_manager::PasswordForm>& forms_to_check,
    const std::u16string& new_username) {
  // In case the username changed, make sure that there exists no other
  // credential with the same signon_realm and username in the same store.
  auto has_conflicting_username = [&forms_to_check,
                                   &new_username](const auto& pair) {
    const password_manager::PasswordForm form = pair.second;
    return new_username == form.username_value &&
           base::ranges::any_of(forms_to_check, [&form](const auto& old_form) {
             return form.signon_realm == old_form.signon_realm &&
                    form.IsUsingAccountStore() ==
                        old_form.IsUsingAccountStore();
           });
  };
  return base::ranges::any_of(key_to_forms, has_conflicting_username);
}

password_manager::PasswordForm GenerateFormFromCredential(
    password_manager::CredentialUIEntry credential,
    password_manager::PasswordForm::Type type) {
  password_manager::PasswordForm form;
  form.url = credential.GetURL();
  form.signon_realm = credential.GetFirstSignonRealm();
  form.username_value = credential.username;
  form.password_value = credential.password;
  form.type = type;
  form.date_created = base::Time::Now();
  form.date_password_modified = form.date_created;

  if (!credential.note.empty())
    form.SetNoteWithEmptyUniqueDisplayName(credential.note);

  DCHECK(!credential.stored_in.empty());
  form.in_store = *credential.stored_in.begin();
  return form;
}

// Check if notes was modified for a specified |form| with |new_note|.
IsPasswordNoteChanged IsNoteChanged(const password_manager::PasswordForm& form,
                                    const std::u16string& new_note) {
  return IsPasswordNoteChanged(
      form.GetNoteWithEmptyUniqueDisplayName().value_or(std::u16string()) !=
      new_note);
}

PasswordNoteAction NoteChangeResultToPasswordNoteEditDialogAction(
    password_manager::PasswordNoteChangeResult result) {
  switch (result) {
    case password_manager::PasswordNoteChangeResult::kNoteAdded:
      return PasswordNoteAction::kNoteAddedInEditDialog;
    case password_manager::PasswordNoteChangeResult::kNoteEdited:
      return PasswordNoteAction::kNoteEditedInEditDialog;
    case password_manager::PasswordNoteChangeResult::kNoteRemoved:
      return PasswordNoteAction::kNoteRemovedInEditDialog;
    case password_manager::PasswordNoteChangeResult::kNoteNotChanged:
      return PasswordNoteAction::kNoteNotChanged;
  }
  NOTREACHED();
  return PasswordNoteAction::kNoteNotChanged;
}

}  // namespace

namespace password_manager {

SavedPasswordsPresenter::SavedPasswordsPresenter(
    AffiliationService* affiliation_service,
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)),
      affiliation_service_(affiliation_service),
      undo_helper_(std::make_unique<PasswordUndoHelper>(profile_store_.get(),
                                                        account_store_.get())) {
  DCHECK(profile_store_);
  DCHECK(affiliation_service_);
}

SavedPasswordsPresenter::~SavedPasswordsPresenter() {
  RemoveObservers();
}

void SavedPasswordsPresenter::Init() {
  // Clear old cache.
  sort_key_to_password_forms_.clear();

  profile_store_->AddObserver(this);
  if (account_store_)
    account_store_->AddObserver(this);
  pending_store_updates++;
  profile_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
  if (account_store_) {
    pending_store_updates++;
    account_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
        weak_ptr_factory_.GetWeakPtr());
  }
}

bool SavedPasswordsPresenter::IsWaitingForPasswordStore() const {
  return pending_store_updates != 0;
}

void SavedPasswordsPresenter::RemoveObservers() {
  if (account_store_)
    account_store_->RemoveObserver(this);
  profile_store_->RemoveObserver(this);
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

SavedPasswordsPresenter::AddResult
SavedPasswordsPresenter::GetExpectedAddResult(
    const CredentialUIEntry& credential) const {
  if (!password_manager_util::IsValidPasswordURL(credential.GetURL()))
    return AddResult::kInvalid;
  if (credential.password.empty())
    return AddResult::kInvalid;

  auto have_equal_username_and_realm =
      [&credential](const PasswordForm& entry) {
        return credential.GetFirstSignonRealm() == entry.signon_realm &&
               credential.username == entry.username_value;
      };
  auto have_equal_username_and_realm_in_profile_store =
      [&have_equal_username_and_realm](
          const DuplicatePasswordsMap::value_type& pair) {
        return have_equal_username_and_realm(pair.second) &&
               pair.second.IsUsingProfileStore();
      };
  auto have_equal_username_and_realm_in_account_store =
      [&have_equal_username_and_realm](
          const DuplicatePasswordsMap::value_type& pair) {
        return have_equal_username_and_realm(pair.second) &&
               pair.second.IsUsingAccountStore();
      };

  bool existing_credential_profile =
      base::ranges::any_of(sort_key_to_password_forms_,
                           have_equal_username_and_realm_in_profile_store);
  bool existing_credential_account =
      base::ranges::any_of(sort_key_to_password_forms_,
                           have_equal_username_and_realm_in_account_store);

  if (!existing_credential_profile && !existing_credential_account)
    return AddResult::kSuccess;

  auto have_exact_match = [&credential, &have_equal_username_and_realm](
                              const DuplicatePasswordsMap::value_type& pair) {
    return have_equal_username_and_realm(pair.second) &&
           credential.password == pair.second.password_value;
  };

  if (base::ranges::any_of(sort_key_to_password_forms_, have_exact_match))
    return AddResult::kExactMatch;

  if (!existing_credential_profile)
    return AddResult::kConflictInAccountStore;
  if (!existing_credential_account)
    return AddResult::kConflictInProfileStore;

  return AddResult::kConflictInProfileAndAccountStore;
}

void SavedPasswordsPresenter::AddCredentialAsync(
    const CredentialUIEntry& credential,
    password_manager::PasswordForm::Type type,
    base::OnceClosure completion) {
  DCHECK_EQ(GetExpectedAddResult(credential), AddResult::kSuccess);

  UnblocklistBothStores(credential);
  PasswordForm form = GenerateFormFromCredential(credential, type);

  GetStoreFor(form).AddLogin(form, base::BindOnce(std::move(completion)));
}

bool SavedPasswordsPresenter::AddCredential(
    const CredentialUIEntry& credential,
    password_manager::PasswordForm::Type type) {
  if (GetExpectedAddResult(credential) != AddResult::kSuccess)
    return false;

  UnblocklistBothStores(credential);
  PasswordForm form = GenerateFormFromCredential(credential, type);

  GetStoreFor(form).AddLogin(form);

  if (form.type == password_manager::PasswordForm::Type::kManuallyAdded) {
    if (!form.notes.empty() && form.notes[0].value.length() > 0) {
      password_manager::metrics_util::LogPasswordNoteActionInSettings(
          PasswordNoteAction::kNoteAddedInAddDialog);
    }
  }

  return true;
}

void SavedPasswordsPresenter::UnblocklistBothStores(
    const CredentialUIEntry& credential) {
  // Try to unblocklist in both stores anyway because if credentials don't
  // exist, the unblocklist operation is no-op.
  auto form_digest =
      PasswordFormDigest(PasswordForm::Scheme::kHtml,
                         credential.GetFirstSignonRealm(), credential.GetURL());
  profile_store_->Unblocklist(form_digest);
  if (account_store_)
    account_store_->Unblocklist(form_digest);
}

void SavedPasswordsPresenter::AddCredentials(
    const std::vector<CredentialUIEntry>& credentials,
    password_manager::PasswordForm::Type type,
    AddCredentialsCallback completion) {
  std::vector<AddResult> results;
  results.reserve(credentials.size());

  // Invalid credentials are filtered out since AddCredentialAsync() won't
  // perform any checks on the credential and expects a valid credential.
  std::vector<CredentialUIEntry> valid_credentials;
  valid_credentials.reserve(credentials.size());

  base::ranges::transform(credentials, std::back_inserter(results),
                          [&](const CredentialUIEntry& credential) {
                            AddResult result = GetExpectedAddResult(credential);
                            if (result == AddResult::kSuccess)
                              valid_credentials.push_back(credential);
                            return result;
                          });

  if (valid_credentials.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion), std::move(results)));
    return;
  }

  if (valid_credentials.size() == 1) {
    AddCredentialAsync(
        std::move(valid_credentials[0]), type,
        base::BindOnce(std::move(completion), std::move(results)));
    return;
  }

  // To avoid multiple updates for the observers we remove them at the
  // beginning.
  RemoveObservers();

  // Reinitialize presenter after all add operations are complete.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      valid_credentials.size(),
      base::BindOnce(&SavedPasswordsPresenter::Init,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(base::BindOnce(std::move(completion), std::move(results))));

  for (CredentialUIEntry& credential : valid_credentials)
    AddCredentialAsync(std::move(credential), type, barrier_closure);
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
  if (username_changed &&
      IsUsernameAlreadyUsed(sort_key_to_password_forms_, forms_to_change,
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

    if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
      if (note_changed) {
        password_manager::PasswordNoteChangeResult note_change_result =
            new_form.SetNoteWithEmptyUniqueDisplayName(updated_credential.note);
        metrics_util::LogPasswordNoteActionInSettings(
            NoteChangeResultToPasswordNoteEditDialogAction(note_change_result));
      } else {
        metrics_util::LogPasswordNoteActionInSettings(
            PasswordNoteAction::kNoteNotChanged);
      }
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
  }

  // Only change in username or password is interesting for OnEdited listeners.
  if (username_changed || password_changed) {
    NotifyEdited(updated_credential);
  }

  password_manager::metrics_util::LogPasswordEditResult(username_changed,
                                                        password_changed);
  return EditResult::kSuccess;
}

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetSavedCredentials()
    const {
  std::vector<CredentialUIEntry> credentials;

  auto it = sort_key_to_password_forms_.begin();
  while (it != sort_key_to_password_forms_.end()) {
    auto current_key = it->first;
    // Aggregate all passwords for the current key.
    std::vector<PasswordForm> current_passwords_group;
    while (it != sort_key_to_password_forms_.end() &&
           it->first == current_key) {
      current_passwords_group.push_back(it->second);
      ++it;
    }
    credentials.emplace_back(current_passwords_group);
  }
  return credentials;
}

std::vector<AffiliatedGroup> SavedPasswordsPresenter::GetAffiliatedGroups() {
  // Sort affiliated groups.
  std::sort(affiliated_groups_.begin(), affiliated_groups_.end(),
            [](const AffiliatedGroup& lhs, const AffiliatedGroup& rhs) {
              return lhs.GetDisplayName() < rhs.GetDisplayName();
            });
  return affiliated_groups_;
}

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetSavedPasswords()
    const {
  auto credentials = GetSavedCredentials();
  base::EraseIf(credentials, [](const auto& credential) {
    return credential.blocked_by_user || !credential.federation_origin.opaque();
  });
  return credentials;
}

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetBlockedSites() {
  DCHECK(base::FeatureList::IsEnabled(features::kPasswordsGrouping));
  // Sort blocked sites.
  std::sort(password_grouping_info_.blocked_sites.begin(),
            password_grouping_info_.blocked_sites.end());
  return password_grouping_info_.blocked_sites;
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

void SavedPasswordsPresenter::NotifyEdited(
    const CredentialUIEntry& credential) {
  for (auto& observer : observers_)
    observer.OnEdited(credential);
}

void SavedPasswordsPresenter::NotifySavedPasswordsChanged() {
  for (auto& observer : observers_)
    observer.OnSavedPasswordsChanged();
}

void SavedPasswordsPresenter::OnLoginsChanged(
    PasswordStoreInterface* store,
    const PasswordStoreChangeList& changes) {
  std::vector<PasswordForm> forms_to_add;
  std::vector<PasswordForm> forms_to_remove;
  for (const PasswordStoreChange& change : changes) {
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        forms_to_add.push_back(change.form());
        break;
      case PasswordStoreChange::UPDATE:
        forms_to_remove.push_back(change.form());
        forms_to_add.push_back(change.form());
        break;
      case PasswordStoreChange::REMOVE:
        forms_to_remove.push_back(change.form());
        break;
    }
  }

  RemoveForms(forms_to_remove);
  // TODO(crbug.com/1381203): Inject branding info for these credentials.
  AddForms(forms_to_add);
}

void SavedPasswordsPresenter::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  bool is_using_account_store = store == account_store_.get();

  // Remove cached credentials for the current store.
  base::EraseIf(sort_key_to_password_forms_,
                [is_using_account_store](
                    const DuplicatePasswordsMap::value_type& key_to_form) {
                  return key_to_form.second.IsUsingAccountStore() ==
                         is_using_account_store;
                });

  // TODO(crbug.com/1381203): Inject branding info for these credentials.
  AddForms(retained_passwords);
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
  pending_store_updates--;
  DCHECK_GE(pending_store_updates, 0);

  std::vector<PasswordForm> forms;
  for (auto& form : results) {
    forms.push_back(std::move(*form));
  }
  AddForms(forms);
}

void SavedPasswordsPresenter::OnGetAllGroupsResultsFrom(
    const std::vector<GroupedFacets>& groups) {
  // Call grouping algorithm.
  password_grouping_info_ = GroupPasswords(groups, sort_key_to_password_forms_);

  // Update affiliated groups cache.
  affiliated_groups_ =
      GetAffiliatedGroupsWithGroupingInfo(password_grouping_info_);

  NotifySavedPasswordsChanged();
}

PasswordStoreInterface& SavedPasswordsPresenter::GetStoreFor(
    const PasswordForm& form) {
  DCHECK_NE(form.IsUsingAccountStore(), form.IsUsingProfileStore());
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

void SavedPasswordsPresenter::RemoveForms(
    const std::vector<PasswordForm>& forms) {
  for (const auto& form : forms) {
    // ArePasswordFormUniqueKeysEqual doesn't take password into account, this
    // is why |in_store| has to be checked as it's possible to have two
    // PasswordForms with the same unique keys but different passwords if and
    // only if they are from different stores.
    base::EraseIf(
        sort_key_to_password_forms_,
        [&form](const DuplicatePasswordsMap::value_type& key_to_form) {
          return ArePasswordFormUniqueKeysEqual(key_to_form.second, form) &&
                 key_to_form.second.in_store == form.in_store;
        });
  }
}

void SavedPasswordsPresenter::AddForms(const std::vector<PasswordForm>& forms) {
  for (const auto& form : forms) {
    // TODO(crbug.com/1359392): Consider replacing |sort_key_to_password_forms_|
    // when grouping is launched.
    sort_key_to_password_forms_.insert(
        std::make_pair(CreateSortKey(form, IgnoreStore(true)), form));
  }

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordsGrouping)) {
    NotifySavedPasswordsChanged();
    return;
  }

  // Don't notify observers about changes until we group credentials.
  AffiliationService::GroupsCallback groups_callback =
      base::BindOnce(&SavedPasswordsPresenter::OnGetAllGroupsResultsFrom,
                     weak_ptr_factory_.GetWeakPtr());
  affiliation_service_->GetAllGroups(std::move(groups_callback));
}

}  // namespace password_manager
