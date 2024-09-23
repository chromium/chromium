// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"

#include <iterator>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"

namespace password_manager {

namespace {

void InjectAffiliationAndBrandingInformation(
    AffiliatedMatchHelper* match_helper,
    LoginsOrErrorReply callback,
    LoginsResultOrError forms_or_error) {
  if (!match_helper ||
      absl::holds_alternative<PasswordStoreBackendError>(forms_or_error) ||
      absl::get<LoginsResult>(forms_or_error).empty()) {
    std::move(callback).Run(std::move(forms_or_error));
    return;
  }
  match_helper->InjectAffiliationAndBrandingInformation(
      std::move(absl::get<LoginsResult>(forms_or_error)), std::move(callback));
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

void FakePasswordStoreBackend::Clear() {
  stored_passwords_.clear();
}

void FakePasswordStoreBackend::TriggerOnLoginsRetainedForAndroid(
    const std::vector<PasswordForm>& password_forms) {
  stored_passwords_.clear();
  for (const auto& password_form : password_forms) {
    PasswordForm stored_form = password_form;
    stored_form.in_store = is_account_store()
                               ? PasswordForm::Store::kAccountStore
                               : PasswordForm::Store::kProfileStore;
    stored_passwords_[password_form.signon_realm].push_back(
        std::move(stored_form));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(remote_form_changes_received_, std::nullopt));
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

bool FakePasswordStoreBackend::IsAbleToSavePasswords() {
  return true;
}

void FakePasswordStoreBackend::GetAllLoginsAsync(LoginsOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::GetAllLoginsInternal,
                     base::Unretained(this)),
      std::move(callback));
}

void FakePasswordStoreBackend::GetAllLoginsWithAffiliationAndBrandingAsync(
    LoginsOrErrorReply callback) {
  auto injection = base::BindOnce(&InjectAffiliationAndBrandingInformation,
                                  match_helper_, std::move(callback));
  GetAllLoginsAsync(std::move(injection));
}

void FakePasswordStoreBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::GetAutofillableLoginsInternal,
                     base::Unretained(this)),
      std::move(callback));
}

void FakePasswordStoreBackend::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  CHECK(!account.empty());
  GetAllLoginsAsync(std::move(callback));
}

void FakePasswordStoreBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
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
    LoginsOrErrorReply callback) {
  GetLoginsWithAffiliationsRequestHandler(form_digest, this, match_helper_,
                                          std::move(callback));
}

void FakePasswordStoreBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::AddLoginInternal,
                     base::Unretained(this), form),
      std::move(callback));
}

void FakePasswordStoreBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::UpdateLoginInternal,
                     base::Unretained(this), form),
      std::move(callback));
}

void FakePasswordStoreBackend::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FakePasswordStoreBackend::RemoveLoginInternal,
                     base::Unretained(this), form),
      std::move(callback));
}

void FakePasswordStoreBackend::RemoveLoginsByURLAndTimeAsync(
    const base::Location& location,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  auto cb = sync_completion ? std::move(callback).Then(base::BindOnce(
                                  std::move(sync_completion), true))
                            : std::move(callback);
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &FakePasswordStoreBackend::RemoveLoginsByURLAndTimeInternal,
          base::Unretained(this), url_filter, delete_begin, delete_end),
      std::move(cb));
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

void FakePasswordStoreBackend::RecordAddLoginAsyncCalledFromTheStore() {}

void FakePasswordStoreBackend::RecordUpdateLoginAsyncCalledFromTheStore() {}

base::WeakPtr<PasswordStoreBackend> FakePasswordStoreBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

LoginsResult FakePasswordStoreBackend::GetAllLoginsInternal() {
  LoginsResult result;
  for (const auto& elements : stored_passwords_) {
    for (const auto& stored_form : elements.second) {
      result.push_back(stored_form);
    }
  }
  return result;
}

LoginsResult FakePasswordStoreBackend::GetAutofillableLoginsInternal() {
  LoginsResult result;
  for (const auto& elements : stored_passwords_) {
    for (const auto& stored_form : elements.second) {
      if (!stored_form.blocked_by_user) {
        result.push_back(stored_form);
      }
    }
  }
  return result;
}

LoginsResult FakePasswordStoreBackend::FillMatchingLoginsInternal(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  LoginsResult results;
  for (const auto& form : forms) {
    LoginsResult matched_forms = FillMatchingLoginsHelper(form, include_psl);
    results.insert(results.end(),
                   std::make_move_iterator(matched_forms.begin()),
                   std::make_move_iterator(matched_forms.end()));
  }
  return results;
}

LoginsResult FakePasswordStoreBackend::FillMatchingLoginsHelper(
    const PasswordFormDigest& form,
    bool include_psl) {
  // Updating all matched forms is the equivalent of FillMatchingLogins();
  LoginsResult matched_forms;
  for (const auto& elements : stored_passwords_) {
    // The code below doesn't support PSL federated credential. It's doable but
    // no tests need it so far.
    const bool realm_matches = elements.first == form.signon_realm;
    const bool realm_psl_matches =
        IsPublicSuffixDomainMatch(elements.first, form.signon_realm);
    if (realm_matches || (realm_psl_matches && include_psl) ||
        (form.scheme == PasswordForm::Scheme::kHtml &&
         password_manager::IsFederatedRealm(elements.first, form.url))) {
      for (const auto& stored_form : elements.second) {
        // Repeat the condition above with an additional check for origin.
        if (realm_matches || realm_psl_matches ||
            (form.scheme == PasswordForm::Scheme::kHtml &&
             stored_form.url.DeprecatedGetOriginAsURL() ==
                 form.url.DeprecatedGetOriginAsURL() &&
             password_manager::IsFederatedRealm(stored_form.signon_realm,
                                                form.url))) {
          matched_forms.push_back(stored_form);
        }
      }
    }
  }
  return matched_forms;
}

PasswordStoreChangeList FakePasswordStoreBackend::AddLoginInternal(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  auto& passwords_for_signon_realm = stored_passwords_[form.signon_realm];
  auto iter = base::ranges::find_if(
      passwords_for_signon_realm, [&form](const auto& password) {
        return ArePasswordFormUniqueKeysEqual(form, password);
      });

  if (iter != passwords_for_signon_realm.end()) {
    changes.emplace_back(PasswordStoreChange::REMOVE, *iter);
    changes.emplace_back(PasswordStoreChange::ADD, form);
    *iter = form;
    iter->in_store = is_account_store() ? PasswordForm::Store::kAccountStore
                                        : PasswordForm::Store::kProfileStore;
    return changes;
  }

  changes.emplace_back(PasswordStoreChange::ADD, form);
  passwords_for_signon_realm.push_back(form);
  passwords_for_signon_realm.back().in_store =
      is_account_store() ? PasswordForm::Store::kAccountStore
                         : PasswordForm::Store::kProfileStore;
  return changes;
}

PasswordStoreChangeList FakePasswordStoreBackend::UpdateLoginInternal(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<PasswordForm>& forms = stored_passwords_[form.signon_realm];
  for (auto& stored_form : forms) {
    if (ArePasswordFormUniqueKeysEqual(form, stored_form)) {
      bool password_changed = form.password_value != stored_form.password_value;
      bool insecure_credentials_changed = false;

      for (auto insecure_type : {InsecureType::kLeaked, InsecureType::kPhished,
                                 InsecureType::kWeak, InsecureType::kReused}) {
        if (form.password_issues.contains(insecure_type) !=
            stored_form.password_issues.contains(insecure_type)) {
          insecure_credentials_changed = true;
          break;
        }
      }

      stored_form = form;
      stored_form.in_store = is_account_store()
                                 ? PasswordForm::Store::kAccountStore
                                 : PasswordForm::Store::kProfileStore;
      changes.emplace_back(
          PasswordStoreChange::UPDATE, form, password_changed,
          InsecureCredentialsChanged(insecure_credentials_changed));
    }
  }
  if (changes.empty() && update_always_succeeds_) {
    changes = AddLoginInternal(form);
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
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<PasswordForm>& forms = stored_passwords_[form.signon_realm];
  auto it = forms.begin();
  while (it != forms.end()) {
    if (ArePasswordFormUniqueKeysEqual(form, *it)) {
      it = forms.erase(it);
      changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    } else {
      ++it;
    }
  }
  if (forms.empty()) {
    stored_passwords_.erase(form.signon_realm);
  }
  return changes;
}

PasswordStoreChangeList
FakePasswordStoreBackend::RemoveLoginsByURLAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  std::vector<PasswordForm> all_logins = GetAllLoginsInternal();
  PasswordStoreChangeList list;
  for (const auto& form : all_logins) {
    if (url_filter.Run(form.url) && delete_begin <= form.date_created &&
        form.date_created < delete_end) {
      base::ranges::move(RemoveLoginInternal(form), std::back_inserter(list));
    }
  }
  return list;
}

PasswordStoreChangeList
FakePasswordStoreBackend::RemoveLoginsCreatedBetweenInternal(
    base::Time delete_begin,
    base::Time delete_end) {
  return RemoveLoginsByURLAndTimeInternal(
      base::BindRepeating([](const GURL&) { return true; }), delete_begin,
      delete_end);
}

}  // namespace password_manager
