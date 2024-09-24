// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/password_undo_helper.h"
#include "components/password_manager/core/browser/ui/passwords_grouper.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "url/gurl.h"

namespace {

using IsUsernameChanged = base::StrongAlias<class IsUsernameChangedTag, bool>;
using IsDisplayNameChanged =
    base::StrongAlias<class IsDisplayNameChangedTag, bool>;
using IsPasswordChanged = base::StrongAlias<class IsPasswordChangedTag, bool>;
using IsPasswordNoteChanged =
    base::StrongAlias<class IsPasswordNoteChangedTag, bool>;
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

  if (!credential.note.empty()) {
    form.SetNoteWithEmptyUniqueDisplayName(credential.note);
  }

  DCHECK(!credential.stored_in.empty());
  form.in_store = *credential.stored_in.begin();
  return form;
}

password_manager::PasswordStoreChangeList GetChangesForAddedForms(
    const std::vector<password_manager::PasswordForm>& forms) {
  password_manager::PasswordStoreChangeList changes;
  for (const auto& form : forms) {
    changes.emplace_back(password_manager::PasswordStoreChange::ADD, form);
  }
  return changes;
}

bool MergeDeleteAllResultsFromPasswordStores(std::vector<bool> results) {
  return base::ranges::all_of(results, [](bool result) { return result; });
}

}  // namespace

namespace password_manager {

SavedPasswordsPresenter::SavedPasswordsPresenter(
    affiliations::AffiliationService* affiliation_service,
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store,
    webauthn::PasskeyModel* passkey_store)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)),
      passkey_store_(passkey_store),
      undo_helper_(std::make_unique<PasswordUndoHelper>(profile_store_.get(),
                                                        account_store_.get())),
      passwords_grouper_(
          std::make_unique<PasswordsGrouper>(affiliation_service)) {}

SavedPasswordsPresenter::~SavedPasswordsPresenter() = default;

void SavedPasswordsPresenter::Init(base::OnceClosure completion_callback) {
  init_completion_callback_ = std::move(completion_callback);

  // Clear old cache.
  sort_key_to_password_forms_.clear();
  passwords_grouper_->ClearCache();

  // Password store is not supported in some configurations.
  if (profile_store_ == nullptr) {
    return;
  }

  profile_store_observation_.Observe(profile_store_.get());
  if (account_store_) {
    account_store_observation_.Observe(account_store_.get());
  }
  if (passkey_store_) {
    passkey_store_observation_.Observe(passkey_store_);
  }
  pending_store_updates_++;
  profile_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
  if (account_store_) {
    pending_store_updates_++;
    account_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
        weak_ptr_factory_.GetWeakPtr());
  }
}

bool SavedPasswordsPresenter::IsWaitingForPasswordStore() const {
  return pending_store_updates_ != 0;
}

bool SavedPasswordsPresenter::RemoveCredential(
    const CredentialUIEntry& credential) {
  if (!credential.passkey_credential_id.empty()) {
    CHECK(passkey_store_);
    std::string credential_id(credential.passkey_credential_id.begin(),
                              credential.passkey_credential_id.end());
    if (!passkey_store_->DeletePasskey(std::move(credential_id), FROM_HERE)) {
      return false;
    }
    return true;
  }
  std::vector<PasswordForm> forms_to_delete =
      GetCorrespondingPasswordForms(credential);
  undo_helper_->StartGroupingActions();
  for (const auto& current_form : forms_to_delete) {
    // Make sure |credential| and |current_form| share the same store.
    if (credential.stored_in.contains(current_form.in_store)) {
      // |current_form| is unchanged result obtained from
      // 'OnGetPasswordStoreResultsFrom'. So it can be present only in one
      // store at a time.
      GetStoreFor(current_form).RemoveLogin(FROM_HERE, current_form);
      undo_helper_->PasswordRemoved(current_form);
    }
  }
  undo_helper_->EndGroupingActions();
  return !forms_to_delete.empty();
}

void SavedPasswordsPresenter::DeleteAllData(
    base::OnceCallback<void(bool)> success_callback) {
  // Synchronosly remove all passkeys if they are available.
  if (passkey_store_) {
    passkey_store_->DeleteAllPasskeys();
  }

  const auto completion_barrier = base::BarrierCallback<bool>(
      2 - !profile_store_ - !account_store_,
      base::BindOnce(&MergeDeleteAllResultsFromPasswordStores)
          .Then(std::move(success_callback)));

  if (account_store_) {
    account_store_->RemoveLoginsCreatedBetween(
        FROM_HERE, base::Time(), base::Time::Max(), completion_barrier);
  }
  if (profile_store_) {
    profile_store_->RemoveLoginsCreatedBetween(
        FROM_HERE, base::Time(), base::Time::Max(), completion_barrier);
  }
}

void SavedPasswordsPresenter::UndoLastRemoval() {
  undo_helper_->Undo();
}

SavedPasswordsPresenter::AddResult
SavedPasswordsPresenter::GetExpectedAddResult(
    const CredentialUIEntry& credential) const {
  if (!IsValidPasswordURL(credential.GetURL())) {
    return AddResult::kInvalid;
  }
  if (credential.password.empty()) {
    return AddResult::kInvalid;
  }

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

  if (!existing_credential_profile && !existing_credential_account) {
    return AddResult::kSuccess;
  }

  auto have_exact_match = [&credential, &have_equal_username_and_realm](
                              const DuplicatePasswordsMap::value_type& pair) {
    return have_equal_username_and_realm(pair.second) &&
           credential.password == pair.second.password_value;
  };

  if (base::ranges::any_of(sort_key_to_password_forms_, have_exact_match)) {
    return AddResult::kExactMatch;
  }

  if (!existing_credential_profile) {
    return AddResult::kConflictInAccountStore;
  }
  if (!existing_credential_account) {
    return AddResult::kConflictInProfileStore;
  }

  return AddResult::kConflictInProfileAndAccountStore;
}

bool SavedPasswordsPresenter::AddCredential(
    const CredentialUIEntry& credential,
    password_manager::PasswordForm::Type type) {
  if (GetExpectedAddResult(credential) != AddResult::kSuccess) {
    return false;
  }

  UnblocklistBothStores(credential);
  PasswordForm form = GenerateFormFromCredential(credential, type);

  GetStoreFor(form).AddLogin(form);
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
  if (account_store_) {
    account_store_->Unblocklist(form_digest);
  }
}

void SavedPasswordsPresenter::AddCredentials(
    const std::vector<CredentialUIEntry>& credentials,
    password_manager::PasswordForm::Type type,
    AddCredentialsCallback completion) {
  if (credentials.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(completion));
    return;
  }

  std::vector<PasswordForm> password_forms;
  base::ranges::transform(credentials, std::back_inserter(password_forms),
                          [&](const CredentialUIEntry& credential) {
                            return GenerateFormFromCredential(credential, type);
                          });

  CHECK(base::ranges::all_of(password_forms, [&](const PasswordForm& form) {
    return password_forms[0].in_store == form.in_store;
  }));

  GetStoreFor(password_forms[0])
      .AddLogins(password_forms, std::move(completion));
}

void SavedPasswordsPresenter::UpdatePasswordForms(
    const std::vector<PasswordForm>& password_forms,
    base::OnceClosure completion) {
  if (password_forms.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(completion));
    return;
  }

  CHECK(base::ranges::all_of(password_forms, [&](const PasswordForm& form) {
    return password_forms[0].in_store == form.in_store;
  }));

  GetStoreFor(password_forms[0])
      .UpdateLogins(password_forms, std::move(completion));
}

SavedPasswordsPresenter::EditResult
SavedPasswordsPresenter::EditSavedCredentials(
    const CredentialUIEntry& original_credential,
    const CredentialUIEntry& updated_credential) {
  if (original_credential.passkey_credential_id.empty()) {
    return EditPassword(original_credential, updated_credential);
  } else {
    return EditPasskey(updated_credential);
  }
}

void SavedPasswordsPresenter::MoveCredentialsToAccount(
    const std::vector<CredentialUIEntry>& credentials,
    metrics_util::MoveToAccountStoreTrigger trigger) {
  for (const auto& credential : credentials) {
    std::vector<PasswordForm> move_form_candidates =
        GetCorrespondingPasswordForms(credential);
    // signon_realms of PasswordForms which are saved in account.
    auto account_credentials_signon_realms = base::MakeFlatSet<std::string>(
        move_form_candidates, {}, [](const auto& form) {
          return form.IsUsingAccountStore() ? form.signon_realm : "";
        });

    for (const auto& form : move_form_candidates) {
      if (form.IsUsingAccountStore()) {
        continue;
      }
      CHECK(form.IsUsingProfileStore());

      // Don't call AddLogin() if the credential already exists in the account
      // store, 1) to avoid unnecessary sync cycles, 2) to avoid potential
      // last_used_date update.
      if (!account_credentials_signon_realms.contains(form.signon_realm)) {
        account_store_->AddLogin(form);
      }
      profile_store_->RemoveLogin(FROM_HERE, form);
    }
  }

  base::UmaHistogramEnumeration(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2",
      trigger);
}

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetSavedCredentials()
    const {
#if BUILDFLAG(IS_ANDROID)
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
#else
  return passwords_grouper_->GetAllCredentials();
#endif
}

std::vector<AffiliatedGroup> SavedPasswordsPresenter::GetAffiliatedGroups() {
  return passwords_grouper_->GetAffiliatedGroupsWithGroupingInfo();
}

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetSavedPasswords()
    const {
  auto credentials = GetSavedCredentials();
  std::erase_if(credentials, [](const auto& credential) {
    return !credential.passkey_credential_id.empty() ||
           credential.blocked_by_user || credential.federation_origin.IsValid();
  });
  return credentials;
}

std::vector<CredentialUIEntry> SavedPasswordsPresenter::GetBlockedSites() {
  return passwords_grouper_->GetBlockedSites();
}

std::vector<PasswordForm>
SavedPasswordsPresenter::GetCorrespondingPasswordForms(
    const CredentialUIEntry& credential) const {
  std::vector<PasswordForm> forms;
#if BUILDFLAG(IS_ANDROID)
  const auto range =
      sort_key_to_password_forms_.equal_range(CreateSortKey(credential));
  base::ranges::transform(range.first, range.second, std::back_inserter(forms),
                          [](const auto& pair) { return pair.second; });
#else
  forms = passwords_grouper_->GetPasswordFormsFor(credential);
#endif
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
  for (auto& observer : observers_) {
    observer.OnEdited(credential);
  }
}

void SavedPasswordsPresenter::NotifySavedPasswordsChanged(
    const PasswordStoreChangeList& changes) {
  // Notify observers when there are no pending password store updates.
  if (pending_store_updates_ > 0) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnSavedPasswordsChanged(changes);
  }

  if (init_completion_callback_) {
    std::move(init_completion_callback_).Run();
  }
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
  // TODO(crbug.com/40876661): Inject branding info for these credentials.
  AddForms(forms_to_add,
           base::BindOnce(&SavedPasswordsPresenter::NotifySavedPasswordsChanged,
                          weak_ptr_factory_.GetWeakPtr(), changes));
}

void SavedPasswordsPresenter::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  bool is_using_account_store = store == account_store_.get();

  // Remove cached credentials for the current store.
  std::erase_if(sort_key_to_password_forms_,
                [is_using_account_store](
                    const DuplicatePasswordsMap::value_type& key_to_form) {
                  return key_to_form.second.IsUsingAccountStore() ==
                         is_using_account_store;
                });

  // TODO(crbug.com/40876661): Inject branding info for these credentials.
  AddForms(retained_passwords,
           base::BindOnce(&SavedPasswordsPresenter::NotifySavedPasswordsChanged,
                          weak_ptr_factory_.GetWeakPtr(),
                          PasswordStoreChangeList()));
}

void SavedPasswordsPresenter::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  MaybeGroupCredentials(base::BindOnce(
      &SavedPasswordsPresenter::NotifySavedPasswordsChanged,
      weak_ptr_factory_.GetWeakPtr(), PasswordStoreChangeList()));
}

void SavedPasswordsPresenter::OnPasskeyModelShuttingDown() {
  passkey_store_observation_.Reset();
}

void SavedPasswordsPresenter::OnPasskeyModelIsReady(bool is_ready) {}

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
  pending_store_updates_--;
  DCHECK_GE(pending_store_updates_, 0);

  std::vector<PasswordForm> forms;
  for (auto& form : results) {
    forms.push_back(std::move(*form));
  }
  AddForms(forms,
           base::BindOnce(&SavedPasswordsPresenter::NotifySavedPasswordsChanged,
                          weak_ptr_factory_.GetWeakPtr(),
                          GetChangesForAddedForms(forms)));
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
    std::erase_if(
        sort_key_to_password_forms_,
        [&form](const DuplicatePasswordsMap::value_type& key_to_form) {
          return ArePasswordFormUniqueKeysEqual(key_to_form.second, form) &&
                 key_to_form.second.in_store == form.in_store;
        });
  }
}

void SavedPasswordsPresenter::AddForms(const std::vector<PasswordForm>& forms,
                                       base::OnceClosure completion) {
  for (const auto& form : forms) {
    // TODO(crbug.com/40862365): Consider replacing
    // |sort_key_to_password_forms_| when grouping is launched.
    sort_key_to_password_forms_.insert(
        std::make_pair(CreateSortKey(CredentialUIEntry(form)), form));
  }

#if BUILDFLAG(IS_ANDROID)
  // Passwords grouping is disabled on Android.
  std::move(completion).Run();
#else
  MaybeGroupCredentials(std::move(completion));
#endif
}

void SavedPasswordsPresenter::MaybeGroupCredentials(
    base::OnceClosure completion) {
  // Group credentials once we received forms from all password stores.
  if (pending_store_updates_ > 0) {
    return;
  }
  // TODO(crbug.com/40858918): Pass only added forms to |passwords_grouper_|.
  std::vector<PasswordForm> all_forms;
  all_forms.reserve(sort_key_to_password_forms_.size());
  for (auto const& [key, form] : sort_key_to_password_forms_) {
    all_forms.push_back(form);
  }

  // Passkeys are collected synchronously.
  std::vector<PasskeyCredential> passkeys;
#if !BUILDFLAG(IS_ANDROID)
  if (passkey_store_) {
    passkeys = PasskeyCredential::FromCredentialSpecifics(
        passkey_store_->GetAllPasskeys());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Notify observers after grouping is complete.
  passwords_grouper_->GroupCredentials(
      std::move(all_forms), std::move(passkeys),
      metrics_util::TimeCallback(std::move(completion),
                                 "PasswordManager.PasswordsGrouping.Time"));
}

SavedPasswordsPresenter::EditResult SavedPasswordsPresenter::EditPasskey(
    const CredentialUIEntry& updated_credential) {
  CHECK(!updated_credential.passkey_credential_id.empty());
  CHECK(passkey_store_);
  std::optional<PasskeyCredential> original_credential =
      passwords_grouper_->GetPasskeyFor(updated_credential);
  if (!original_credential) {
    return EditResult::kNotFound;
  }
  std::string new_username = base::UTF16ToUTF8(updated_credential.username);
  std::string new_display_name =
      base::UTF16ToUTF8(updated_credential.user_display_name);
  IsUsernameChanged username_changed(new_username !=
                                     original_credential->username());
  IsDisplayNameChanged display_name_changed(
      new_display_name != original_credential->display_name());
  if (!username_changed && !display_name_changed) {
    return EditResult::kNothingChanged;
  }
  std::string credential_id(original_credential->credential_id().begin(),
                            original_credential->credential_id().end());
  passkey_store_->UpdatePasskey(
      credential_id,
      {
          .user_name = std::move(new_username),
          .user_display_name = std::move(new_display_name),
      },
      /*updated_by_user=*/true);
  return EditResult::kSuccess;
}

SavedPasswordsPresenter::EditResult SavedPasswordsPresenter::EditPassword(
    const CredentialUIEntry& original_credential,
    const CredentialUIEntry& updated_credential) {
  std::vector<PasswordForm> forms_to_change =
      GetCorrespondingPasswordForms(original_credential);
  if (forms_to_change.empty()) {
    return EditResult::kNotFound;
  }

  IsUsernameChanged username_changed(updated_credential.username !=
                                     original_credential.username);
  IsPasswordChanged password_changed(updated_credential.password !=
                                     original_credential.password);
  IsPasswordNoteChanged note_changed(
      forms_to_change[0].GetNoteWithEmptyUniqueDisplayName() !=
      updated_credential.note);
  bool issues_changed =
      updated_credential.password_issues != forms_to_change[0].password_issues;

  // Password can't be empty.
  if (updated_credential.password.empty()) {
    return EditResult::kEmptyPassword;
  }

  // Username can't be changed to the existing one.
  if (username_changed &&
      IsUsernameAlreadyUsed(sort_key_to_password_forms_, forms_to_change,
                            updated_credential.username)) {
    return EditResult::kAlreadyExisits;
  }

  // Nothing changed.
  if (!username_changed && !password_changed && !note_changed &&
      !issues_changed) {
    return EditResult::kNothingChanged;
  }

  base::RepeatingClosure completion_barrier_closure = base::DoNothing();
  // Only change in username or password is interesting for OnEdited listeners.
  if (username_changed || password_changed) {
    completion_barrier_closure = base::BarrierClosure(
        forms_to_change.size(),
        base::BindOnce(&SavedPasswordsPresenter::NotifyEdited,
                       weak_ptr_factory_.GetWeakPtr(), updated_credential));
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
      new_form.SetNoteWithEmptyUniqueDisplayName(updated_credential.note);
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
      store.UpdateLoginWithPrimaryKey(new_form, old_form,
                                      completion_barrier_closure);
    } else {
      store.UpdateLogin(new_form, completion_barrier_closure);
    }
  }

  return EditResult::kSuccess;
}

}  // namespace password_manager
