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
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
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

void LogMetricsAddCredential(const password_manager::PasswordForm& form) {
  password_manager::metrics_util::
      LogUserInteractionsWhenAddingCredentialFromSettings(
          password_manager::metrics_util::
              AddCredentialFromSettingsUserInteractions::kCredentialAdded);
  if (!form.notes.empty() && form.notes[0].value.length() > 0) {
    password_manager::metrics_util::LogPasswordNoteActionInSettings(
        PasswordNoteAction::kNoteAddedInAddDialog);
  }
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
  AddObservers();
}

SavedPasswordsPresenter::~SavedPasswordsPresenter() {
  RemoveObservers();
}

void SavedPasswordsPresenter::Init() {
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

void SavedPasswordsPresenter::AddObservers() {
  profile_store_->AddObserver(this);
  if (account_store_)
    account_store_->AddObserver(this);
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
      [&have_equal_username_and_realm](const PasswordForm& entry) {
        return have_equal_username_and_realm(entry) &&
               entry.IsUsingProfileStore();
      };
  auto have_equal_username_and_realm_in_account_store =
      [&have_equal_username_and_realm](const PasswordForm& entry) {
        return have_equal_username_and_realm(entry) &&
               entry.IsUsingAccountStore();
      };

  bool existing_credential_profile = base::ranges::any_of(
      passwords_, have_equal_username_and_realm_in_profile_store);
  bool existing_credential_account = base::ranges::any_of(
      passwords_, have_equal_username_and_realm_in_account_store);

  if (!existing_credential_profile && !existing_credential_account)
    return AddResult::kSuccess;

  auto have_exact_match =
      [&credential, &have_equal_username_and_realm](const PasswordForm& entry) {
        return have_equal_username_and_realm(entry) &&
               credential.password == entry.password_value;
      };

  if (base::ranges::any_of(passwords_, have_exact_match))
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
  LogMetricsAddCredential(form);
}

bool SavedPasswordsPresenter::AddCredential(
    const CredentialUIEntry& credential,
    password_manager::PasswordForm::Type type) {
  if (GetExpectedAddResult(credential) != AddResult::kSuccess)
    return false;

  UnblocklistBothStores(credential);
  PasswordForm form = GenerateFormFromCredential(credential, type);

  GetStoreFor(form).AddLogin(form);
  LogMetricsAddCredential(form);

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
  std::vector<const CredentialUIEntry*> valid_credentials;
  valid_credentials.reserve(credentials.size());

  base::ranges::transform(credentials, std::back_inserter(results),
                          [&](const CredentialUIEntry& credential) {
                            AddResult result = GetExpectedAddResult(credential);
                            if (result == AddResult::kSuccess)
                              valid_credentials.emplace_back(&credential);
                            return result;
                          });

  if (valid_credentials.empty()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion), std::move(results)));
    return;
  }

  if (valid_credentials.size() == 1) {
    AddCredentialAsync(
        *valid_credentials[0], type,
        base::BindOnce(std::move(completion), std::move(results)));
    return;
  }

  // To avoid multiple updates for the observers we remove them at the
  // beginning.
  RemoveObservers();

  // The observers will have an effect only on the Add after enabling them. Thus
  // we need to reenable them already on the n-1 transaction.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      valid_credentials.size() - 1,
      base::BindOnce(&SavedPasswordsPresenter::AddObservers,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(base::BindOnce(
              &SavedPasswordsPresenter::AddCredentialAsync,
              weak_ptr_factory_.GetWeakPtr(), *valid_credentials.back(), type,
              base::BindOnce(std::move(completion), std::move(results)))));

  for (size_t i = 0; i < valid_credentials.size() - 1; i++)
    AddCredentialAsync(*valid_credentials[i], type, barrier_closure);
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

    if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
      if (note_changed) {
        PasswordNoteAction note_action =
            UpdateNoteInPasswordForm(new_form, updated_credential.note);
        metrics_util::LogPasswordNoteActionInSettings(note_action);
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
    NotifyEdited(new_form);
  }

  password_manager::metrics_util::LogPasswordEditResult(username_changed,
                                                        password_changed);
  return EditResult::kSuccess;
}

SavedPasswordsView SavedPasswordsPresenter::GetSavedPasswords() const {
  return passwords_;
}

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetSavedCredentials()
    const {
  std::vector<CredentialUIEntry> credentials;

  auto it = sort_key_to_password_forms_.begin();
  std::string current_key;

  while (it != sort_key_to_password_forms_.end()) {
    if (current_key != it->first) {
      current_key = it->first;
      credentials.emplace_back(it->second);
    } else {
      // Aggregates store information which might be different across copies.
      credentials.back().stored_in.insert(it->second.in_store);
    }
    ++it;
  }

  return credentials;
}

std::vector<AffiliatedGroup> SavedPasswordsPresenter::GetAffiliatedGroups()
    const {
  std::vector<AffiliatedGroup> affiliated_groups;
  // Key: Group id | Value: map of vectors of password forms.
  for (auto const& it : map_group_id_to_forms_) {
    AffiliatedGroup affiliated_group = AffiliatedGroup();

    // Add branding information to the affiliated group.
    auto it2 = map_group_id_to_branding_info_.find(it.first);
    if (it2 != map_group_id_to_branding_info_.end()) {
      affiliated_group.branding_info = it2->second;
    }

    // Key: Username-password key | Value: vector of password forms.
    for (auto const& it3 : it.second) {
      CredentialUIEntry entry = CredentialUIEntry(it3.second);
      affiliated_group.credential_groups.push_back(std::move(entry));
    }
    affiliated_groups.push_back(std::move(affiliated_group));
  }
  return affiliated_groups;
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
  pending_store_updates++;
  store->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
}

void SavedPasswordsPresenter::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  pending_store_updates++;
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
  pending_store_updates--;
  DCHECK_GE(pending_store_updates, 0);

  // Remove cached credentials for current store.
  // TODO(crbug.com/1359392): Remove unused sort_key_to_password_forms_ when the
  // feature is completely released.
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

  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordsGrouping)) {
    if (!is_account_store) {
      // Fetch all groups.
      AffiliationService::GroupsCallback groups_callback =
          base::BindOnce(&SavedPasswordsPresenter::OnGetAllGroupsResultsFrom,
                         weak_ptr_factory_.GetWeakPtr());
      affiliation_service_->GetAllGroups(std::move(groups_callback));
    }
  } else {
    NotifySavedPasswordsChanged();
  }
}

void SavedPasswordsPresenter::OnGetAllGroupsResultsFrom(
    const std::vector<GroupedFacets>& groups) {
  // Clear caches.
  map_group_id_to_branding_info_.clear();
  map_signon_realm_to_group_id_.clear();
  map_group_id_to_forms_.clear();

  // Construct map to keep track of facet URI to group id mapping.
  int group_id_int = 1;
  std::map<std::string, GroupId> map_facet_to_group_id;
  for (const GroupedFacets& grouped_facets : groups) {
    GroupId unique_group_id(group_id_int);
    for (const Facet& facet : grouped_facets.facets) {
      map_facet_to_group_id[facet.uri.canonical_spec()] = unique_group_id;
    }

    // Store branding information for the affiliated group.
    map_group_id_to_branding_info_[unique_group_id] =
        grouped_facets.branding_info;

    // Increment so it is a new id for the next group.
    group_id_int++;
  }

  // Construct a map to keep track of group id to a map of credential groups
  // to password form.
  for (auto const& element : sort_key_to_password_forms_) {
    PasswordForm form = element.second;
    FacetURI uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
    GroupId group_id = map_facet_to_group_id[uri.canonical_spec()];

    // TODO(crbug.com/1354196): If group_id == 0, the password form is not
    // part of an affiliated group that has branding information. Add fallback
    // code here.

    UsernamePasswordKey key(CreateUsernamePasswordSortKey(form));
    map_group_id_to_forms_[group_id][key].push_back(std::move(form));

    // Store group id for sign-on realm.
    SignonRealm signon_realm(uri.canonical_spec());
    map_signon_realm_to_group_id_[signon_realm] = group_id;
  }
  NotifySavedPasswordsChanged();
}

PasswordStoreInterface& SavedPasswordsPresenter::GetStoreFor(
    const PasswordForm& form) {
  DCHECK_NE(form.IsUsingAccountStore(), form.IsUsingProfileStore());
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

}  // namespace password_manager
