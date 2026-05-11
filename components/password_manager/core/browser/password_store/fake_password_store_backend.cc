// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"

namespace password_manager {

namespace {

void InjectAffiliationAndBrandingInformation(
    AffiliatedMatchHelper* match_helper,
    BackendLoginsOrErrorReply callback,
    BackendLoginsResultOrError result) {
  if (!match_helper ||
      std::holds_alternative<PasswordStoreBackendError>(result) ||
      std::get<BackendLoginsResult>(result).empty()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  match_helper->InjectAffiliationAndBrandingInformation(
      std::get<BackendLoginsResult>(std::move(result)), std::move(callback));
}

}  // namespace

FakePasswordStoreBackend::FakePasswordStoreBackend() = default;

FakePasswordStoreBackend::FakePasswordStoreBackend(
    IsAccountStore is_account_store,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : FakePasswordStoreBackend(is_account_store,
                               UpdateAlwaysSucceeds(false),
                               std::move(task_runner)) {}

FakePasswordStoreBackend::FakePasswordStoreBackend(
    IsAccountStore is_account_store,
    UpdateAlwaysSucceeds update_always_succeeds,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : is_account_store_(is_account_store),
      update_always_succeeds_(update_always_succeeds),
      task_runner_(std::move(task_runner)) {}

FakePasswordStoreBackend::~FakePasswordStoreBackend() = default;

const scoped_refptr<base::SequencedTaskRunner>&
FakePasswordStoreBackend::GetTaskRunner() const {
  return task_runner_ ? task_runner_
                      : base::SequencedTaskRunner::GetCurrentDefault();
}

void FakePasswordStoreBackend::TriggerOnLoginsRetainedForAndroid(
    const std::vector<StoredCredential>& credentials) {
  stored_passwords_.clear();
  for (const auto& cred : credentials) {
    StoredCredential stored_cred = CloneStoredCredential(cred);
    stored_cred.in_store = is_account_store()
                               ? PasswordForm::Store::kAccountStore
                               : PasswordForm::Store::kProfileStore;
    stored_passwords_[cred.signon_realm].push_back(std::move(stored_cred));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(remote_form_changes_received_, std::nullopt));
}

void FakePasswordStoreBackend::ReturnErrorOnRequest(
    PasswordStoreBackendError password_store_backend_error) {
  password_store_backend_error_ = password_store_backend_error;
  actionable_error_ =
      BackendErrorToActionableError(password_store_backend_error.type);
}

void FakePasswordStoreBackend::SetError(ActionableError error) {
  actionable_error_ = error;
}

void FakePasswordStoreBackend::NotifyAboutError() {
  if (actionable_error_ == ActionableError::kNoError) {
    remote_form_changes_received_.Run(std::nullopt);
    return;
  }

  PasswordStoreBackendErrorType error_type =
      PasswordStoreBackendErrorType::kUncategorized;
  switch (actionable_error_) {
    case ActionableError::kNoError:
      NOTREACHED();
    case ActionableError::kInactionable:
      error_type = PasswordStoreBackendErrorType::kUncategorized;
      break;
    case ActionableError::kSignInNeeded:
      error_type = PasswordStoreBackendErrorType::kAuthErrorResolvable;
      break;
    case ActionableError::kKeychainError:
      error_type = PasswordStoreBackendErrorType::kKeychainError;
      break;
    case ActionableError::kNeedsPassphrase:
      error_type = PasswordStoreBackendErrorType::kNeedsPassphrase;
      break;
    case ActionableError::kTrustedVaultKeyNeeded:
      error_type = PasswordStoreBackendErrorType::kKeyRetrievalRequired;
      break;
    case ActionableError::kInactionableTemporaryError:
      error_type = PasswordStoreBackendErrorType::kUncategorized;
      break;
  }
  remote_form_changes_received_.Run(PasswordStoreBackendError(error_type));
}

void FakePasswordStoreBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  match_helper_ = affiliated_match_helper;
  remote_form_changes_received_ = std::move(remote_form_changes_received);
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion), /*success=*/true));
}

void FakePasswordStoreBackend::Shutdown(base::OnceClosure shutdown_completed) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  match_helper_ = nullptr;
  // Ensure that the shutdown is only completed after any other backend task on
  // the same task runner concluded. The backend always uses the same runner.
  GetTaskRunner()->PostTask(FROM_HERE, std::move(shutdown_completed));
}

ActionableError FakePasswordStoreBackend::GetError() {
  return actionable_error_;
}

void FakePasswordStoreBackend::GetAllLoginsAsync(
    BackendLoginsOrErrorReply callback) {
  if (password_store_backend_error_.has_value()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  password_store_backend_error_.value()));
  } else {
    GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FakePasswordStoreBackend::GetAllLoginsInternal,
                       base::Unretained(this)),
        std::move(callback));
  }
}

void FakePasswordStoreBackend::GetAllLoginsWithAffiliationAndBrandingAsync(
    BackendLoginsOrErrorReply callback) {
  auto injection = base::BindOnce(&InjectAffiliationAndBrandingInformation,
                                  match_helper_, std::move(callback));
  GetAllLoginsAsync(std::move(injection));
}

void FakePasswordStoreBackend::GetAutofillableLoginsAsync(
    BackendLoginsOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::GetAutofillableLoginsInternal,
                     base::Unretained(this)),
      std::move(callback));
}

void FakePasswordStoreBackend::FillMatchingLoginsAsync(
    BackendLoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::FillMatchingLoginsInternal,
                     base::Unretained(this), forms, include_psl),
      std::move(callback));
}

void FakePasswordStoreBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    BackendLoginsOrErrorReply callback) {
  GetLoginsWithAffiliationsRequestHandler(form_digest, this, match_helper_,
                                          std::move(callback));
}

void FakePasswordStoreBackend::AddLoginAsync(
    StoredCredential cred,
    PasswordChangesOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::AddLoginInternal,
                     base::Unretained(this), std::move(cred)),
      std::move(callback));
}

void FakePasswordStoreBackend::UpdateLoginAsync(
    StoredCredential cred,
    PasswordChangesOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::UpdateLoginInternal,
                     base::Unretained(this), std::move(cred)),
      std::move(callback));
}

void FakePasswordStoreBackend::RemoveLoginAsync(
    const base::Location& location,
    StoredCredential cred,
    PasswordChangesOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::RemoveLoginInternal,
                     base::Unretained(this), std::move(cred)),
      std::move(callback));
}

void FakePasswordStoreBackend::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &FakePasswordStoreBackend::RemoveLoginsCreatedBetweenInternal,
          base::Unretained(this), delete_begin, delete_end),
      std::move(callback));
}

void FakePasswordStoreBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  GetTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &FakePasswordStoreBackend::DisableAutoSignInForOriginsInternal,
          base::Unretained(this), origin_filter),
      std::move(completion));
}

SmartBubbleStatsStore* FakePasswordStoreBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
FakePasswordStoreBackend::CreateSyncControllerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakePasswordStoreBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  NOTIMPLEMENTED();
}

base::WeakPtr<PasswordStoreBackend> FakePasswordStoreBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BackendLoginsResult FakePasswordStoreBackend::GetAllLoginsInternal() {
  BackendLoginsResult result;
  for (const auto& elements : stored_passwords_) {
    for (const auto& stored_cred : elements.second) {
      result.push_back(CloneStoredCredential(stored_cred));
    }
  }
  return result;
}

BackendLoginsResult FakePasswordStoreBackend::GetAutofillableLoginsInternal() {
  BackendLoginsResult result;
  for (const auto& elements : stored_passwords_) {
    for (const auto& stored_cred : elements.second) {
      if (!stored_cred.blocked_by_user) {
        result.push_back(CloneStoredCredential(stored_cred));
      }
    }
  }
  return result;
}

BackendLoginsResult FakePasswordStoreBackend::FillMatchingLoginsInternal(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  BackendLoginsResult results;
  for (const auto& form : forms) {
    BackendLoginsResult matched_creds =
        FillMatchingLoginsHelper(form, include_psl);
    results.insert(results.end(),
                   std::make_move_iterator(matched_creds.begin()),
                   std::make_move_iterator(matched_creds.end()));
  }
  return results;
}

BackendLoginsResult FakePasswordStoreBackend::FillMatchingLoginsHelper(
    const PasswordFormDigest& form,
    bool include_psl) {
  BackendLoginsResult matched_creds;
  for (const auto& elements : stored_passwords_) {
    const bool realm_matches = elements.first == form.signon_realm;
    const bool realm_psl_matches =
        IsPublicSuffixDomainMatch(elements.first, form.signon_realm);
    if (realm_matches || (realm_psl_matches && include_psl) ||
        (form.scheme == PasswordForm::Scheme::kHtml &&
         password_manager::IsFederatedRealm(elements.first, form.url))) {
      for (const auto& stored_cred : elements.second) {
        if (realm_matches || realm_psl_matches ||
            (form.scheme == PasswordForm::Scheme::kHtml &&
             stored_cred.url.DeprecatedGetOriginAsURL() ==
                 form.url.DeprecatedGetOriginAsURL() &&
             password_manager::IsFederatedRealm(stored_cred.signon_realm,
                                                form.url))) {
          matched_creds.push_back(CloneStoredCredential(stored_cred));
        }
      }
    }
  }
  return matched_creds;
}

PasswordStoreChangeList FakePasswordStoreBackend::AddLoginInternal(
    const StoredCredential& cred) {
  PasswordStoreChangeList changes;
  auto& passwords_for_signon_realm = stored_passwords_[cred.signon_realm];
  auto iter = std::ranges::find_if(
      passwords_for_signon_realm, [&cred](const auto& password) {
        return AreStoredCredentialUniqueKeysEqual(cred, password);
      });

  if (iter != passwords_for_signon_realm.end()) {
    changes.emplace_back(PasswordStoreChange::REMOVE,
                         CloneStoredCredential(*iter),
                         /*password_changed=*/false);
    changes.emplace_back(PasswordStoreChange::ADD, CloneStoredCredential(cred),
                         /*password_changed=*/false);
    *iter = CloneStoredCredential(cred);
    iter->in_store = is_account_store() ? PasswordForm::Store::kAccountStore
                                        : PasswordForm::Store::kProfileStore;
    return changes;
  }

  changes.emplace_back(PasswordStoreChange::ADD, CloneStoredCredential(cred),
                       /*password_changed=*/false);
  passwords_for_signon_realm.push_back(CloneStoredCredential(cred));
  passwords_for_signon_realm.back().in_store =
      is_account_store() ? PasswordForm::Store::kAccountStore
                         : PasswordForm::Store::kProfileStore;
  return changes;
}

PasswordStoreChangeList FakePasswordStoreBackend::UpdateLoginInternal(
    const StoredCredential& cred) {
  PasswordStoreChangeList changes;
  std::vector<StoredCredential>& creds = stored_passwords_[cred.signon_realm];
  for (auto& stored_cred : creds) {
    if (AreStoredCredentialUniqueKeysEqual(cred, stored_cred)) {
      bool password_changed = cred.password_value != stored_cred.password_value;
      bool insecure_credentials_changed = false;

      for (auto insecure_type : {InsecureType::kLeaked, InsecureType::kPhished,
                                 InsecureType::kWeak, InsecureType::kReused}) {
        if (cred.password_issues.contains(insecure_type) !=
            stored_cred.password_issues.contains(insecure_type)) {
          insecure_credentials_changed = true;
          break;
        }
      }

      stored_cred = CloneStoredCredential(cred);
      stored_cred.in_store = is_account_store()
                                 ? PasswordForm::Store::kAccountStore
                                 : PasswordForm::Store::kProfileStore;
      changes.emplace_back(
          PasswordStoreChange::UPDATE, CloneStoredCredential(cred),
          password_changed,
          InsecureCredentialsChanged(insecure_credentials_changed));
    }
  }
  if (changes.empty() && update_always_succeeds_) {
    changes = AddLoginInternal(cred);
  }
  return changes;
}

void FakePasswordStoreBackend::DisableAutoSignInForOriginsInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  for (auto& realm : stored_passwords_) {
    if (origin_filter.Run(GURL(realm.first))) {
      for (auto& form : realm.second) {
        form.skip_zero_click = true;
      }
    }
  }
}

PasswordStoreChangeList FakePasswordStoreBackend::RemoveLoginInternal(
    const StoredCredential& cred) {
  PasswordStoreChangeList changes;
  std::vector<StoredCredential>& creds = stored_passwords_[cred.signon_realm];
  auto it = creds.begin();
  while (it != creds.end()) {
    if (AreStoredCredentialUniqueKeysEqual(cred, *it)) {
      it = creds.erase(it);
      changes.emplace_back(PasswordStoreChange::REMOVE,
                           CloneStoredCredential(cred),
                           /*password_changed=*/false);
    } else {
      ++it;
    }
  }
  if (creds.empty()) {
    stored_passwords_.erase(cred.signon_realm);
  }
  return changes;
}

PasswordStoreChangeList
FakePasswordStoreBackend::RemoveLoginsCreatedBetweenInternal(
    base::Time delete_begin,
    base::Time delete_end) {
  BackendLoginsResult all_logins = GetAllLoginsInternal();
  PasswordStoreChangeList list;
  for (const auto& cred : all_logins) {
    if (delete_begin <= cred.date_created && cred.date_created < delete_end) {
      std::ranges::move(RemoveLoginInternal(cred), std::back_inserter(list));
    }
  }
  return list;
}

}  // namespace password_manager
