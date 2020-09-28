// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/compromised_credentials_consumer.h"
#include "components/password_manager/core/browser/compromised_credentials_observer.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/proxy_model_type_controller_delegate.h"

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif

namespace password_manager {

namespace {

// Utility function to simplify removing logins prior a given |cutoff| data.
// Runs |callback| with the result.
//
// TODO(http://crbug.com/121738): Remove this once filtering logins is no longer
// necessary.
void FilterLogins(
    base::Time cutoff,
    base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
        callback,
    std::vector<std::unique_ptr<PasswordForm>> logins) {
  base::EraseIf(logins, [cutoff](const auto& form) {
    return form->date_created < cutoff;
  });

  std::move(callback).Run(std::move(logins));
}

void CloseTraceAndCallBack(
    const char* trace_name,
    PasswordStoreConsumer* consumer,
    base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
        callback,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("passwords", trace_name, consumer);

  std::move(callback).Run(std::move(results));
}

}  // namespace

void PasswordStore::Observer::OnLoginsChangedIn(
    PasswordStore* store,
    const PasswordStoreChangeList& changes) {
  OnLoginsChanged(changes);
}

void PasswordStore::DatabaseCompromisedCredentialsObserver::
    OnCompromisedCredentialsChangedIn(PasswordStore* store) {
  OnCompromisedCredentialsChanged();
}

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
PasswordStore::CheckReuseRequest::CheckReuseRequest(
    PasswordReuseDetectorConsumer* consumer)
    : origin_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      consumer_weak_(consumer->AsWeakPtr()) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords", "CheckReuseRequest", this);
}

PasswordStore::CheckReuseRequest::~CheckReuseRequest() = default;

void PasswordStore::CheckReuseRequest::OnReuseCheckDone(
    bool is_reuse_found,
    size_t password_length,
    base::Optional<PasswordHashData> reused_protected_password_hash,
    const std::vector<MatchingReusedCredential>& matching_reused_credentials,
    int saved_passwords) {
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordReuseDetectorConsumer::OnReuseCheckDone,
                     consumer_weak_, is_reuse_found, password_length,
                     reused_protected_password_hash,
                     matching_reused_credentials, saved_passwords));
  TRACE_EVENT_NESTABLE_ASYNC_END0("passwords", "CheckReuseRequest", this);
}
#endif

PasswordStore::FormDigest::FormDigest(PasswordForm::Scheme new_scheme,
                                      const std::string& new_signon_realm,
                                      const GURL& new_url)
    : scheme(new_scheme), signon_realm(new_signon_realm), url(new_url) {}

PasswordStore::FormDigest::FormDigest(const PasswordForm& form)
    : scheme(form.scheme), signon_realm(form.signon_realm), url(form.url) {}

PasswordStore::FormDigest::FormDigest(const autofill::FormData& form)
    : scheme(PasswordForm::Scheme::kHtml),
      signon_realm(form.url.GetOrigin().spec()),
      url(form.url) {}

PasswordStore::FormDigest::FormDigest(const FormDigest& other) = default;

PasswordStore::FormDigest::FormDigest(FormDigest&& other) = default;

PasswordStore::FormDigest& PasswordStore::FormDigest::operator=(
    const FormDigest& other) = default;

PasswordStore::FormDigest& PasswordStore::FormDigest::operator=(
    FormDigest&& other) = default;

bool PasswordStore::FormDigest::operator==(const FormDigest& other) const {
  return scheme == other.scheme && signon_realm == other.signon_realm &&
         url == other.url;
}

bool PasswordStore::FormDigest::operator!=(const FormDigest& other) const {
  return !(*this == other);
}

PasswordStore::PasswordStore()
    : observers_(new base::ObserverListThreadSafe<Observer>()) {}

bool PasswordStore::Init(PrefService* prefs,
                         base::RepeatingClosure sync_enabled_or_disabled_cb) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  DCHECK(main_task_runner_);
  background_task_runner_ = CreateBackgroundTaskRunner();
  DCHECK(background_task_runner_);
  sync_enabled_or_disabled_cb_ = std::move(sync_enabled_or_disabled_cb);
  prefs_ = prefs;
#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  hash_password_manager_.set_prefs(prefs);
#endif
  if (background_task_runner_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "passwords", "PasswordStore::InitOnBackgroundSequence", this);
    base::PostTaskAndReplyWithResult(
        background_task_runner_.get(), FROM_HERE,
        base::BindOnce(&PasswordStore::InitOnBackgroundSequence, this),
        base::BindOnce(&PasswordStore::OnInitCompleted, this));
  }

  return true;
}

void PasswordStore::SetAffiliatedMatchHelper(
    std::unique_ptr<AffiliatedMatchHelper> helper) {
  affiliated_match_helper_ = std::move(helper);
}

void PasswordStore::AddLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::AddLoginInternal, this, form));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::UpdateLoginInternal, this, form));
}

void PasswordStore::UpdateLoginWithPrimaryKey(
    const PasswordForm& new_form,
    const PasswordForm& old_primary_key) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::UpdateLoginWithPrimaryKeyInternal,
                              this, new_form, old_primary_key));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::RemoveLoginInternal, this, form));
}

void PasswordStore::RemoveLoginsByURLAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion,
    base::OnceCallback<void(bool)> sync_completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::RemoveLoginsByURLAndTimeInternal,
                              this, url_filter, delete_begin, delete_end,
                              std::move(completion),
                              std::move(sync_completion)));
}

void PasswordStore::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                               base::Time delete_end,
                                               base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(
      base::BindOnce(&PasswordStore::RemoveLoginsCreatedBetweenInternal, this,
                     delete_begin, delete_end, std::move(completion)));
}

void PasswordStore::RemoveStatisticsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(
      &PasswordStore::RemoveStatisticsByOriginAndTimeInternal, this,
      origin_filter, delete_begin, delete_end, std::move(completion)));
}

void PasswordStore::DisableAutoSignInForOrigins(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(
      base::BindOnce(&PasswordStore::DisableAutoSignInForOriginsInternal, this,
                     base::RepeatingCallback<bool(const GURL&)>(origin_filter),
                     std::move(completion)));
}

void PasswordStore::Unblacklist(const PasswordStore::FormDigest& form_digest,
                                base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::UnblacklistInternal, this,
                              form_digest, std::move(completion)));
}

void PasswordStore::GetLogins(const FormDigest& form,
                              PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords", "PasswordStore::GetLogins",
                                    consumer);
  // Per http://crbug.com/121738, we deliberately ignore saved logins for
  // http*://www.google.com/ that were stored prior to 2012. (Google now uses
  // https://accounts.google.com/ for all login forms, so these should be
  // unused.) We don't delete them just yet, and they'll still be visible in the
  // password manager, but we won't use them to autofill any forms. This is a
  // security feature to help minimize damage that can be done by XSS attacks.
  // TODO(mdm): actually delete them at some point, say M24 or so.
  base::Time cutoff;  // the null time
  if (form.scheme == PasswordForm::Scheme::kHtml &&
      (form.signon_realm == "http://www.google.com" ||
       form.signon_realm == "http://www.google.com/" ||
       form.signon_realm == "https://www.google.com" ||
       form.signon_realm == "https://www.google.com/")) {
    static const base::Time::Exploded exploded_cutoff = {
        2012, 1, 0, 1, 0, 0, 0, 0};  // 00:00 Jan 1 2012
    base::Time out_time;
    bool conversion_success =
        base::Time::FromUTCExploded(exploded_cutoff, &out_time);
    DCHECK(conversion_success);
    cutoff = out_time;
  }

  if (affiliated_match_helper_) {
    affiliated_match_helper_->GetAffiliatedAndroidRealms(
        form, base::BindOnce(
                  &PasswordStore::ScheduleGetFilteredLoginsWithAffiliations,
                  this, consumer->GetWeakPtr(), form, cutoff));
  } else {
    PostLoginsTaskAndReplyToConsumerWithProcessedResult(
        "PasswordStore::GetLogins", consumer,
        base::BindOnce(&PasswordStore::GetLoginsImpl, this, form),
        base::BindOnce(FilterLogins, cutoff));
  }
}

void PasswordStore::GetLoginsByPassword(
    const base::string16& plain_text_password,
    PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostLoginsTaskAndReplyToConsumerWithResult(
      consumer, base::BindOnce(&PasswordStore::GetLoginsByPasswordImpl, this,
                               plain_text_password));
}

void PasswordStore::GetAutofillableLogins(PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostLoginsTaskAndReplyToConsumerWithResult(
      consumer,
      base::BindOnce(&PasswordStore::GetAutofillableLoginsImpl, this));
}

void PasswordStore::GetAllLogins(PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostLoginsTaskAndReplyToConsumerWithResult(
      consumer, base::BindOnce(&PasswordStore::GetAllLoginsImpl, this));
}

void PasswordStore::GetAllLoginsWithAffiliationAndBrandingInformation(
    PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "passwords",
      "PasswordStore::GetAllLoginsWithAffiliationAndBrandingInformation",
      consumer);
  PostLoginsTaskAndReplyToConsumerWithProcessedResult(
      "PasswordStore::GetAllLoginsWithAffiliationAndBrandingInformation",
      consumer, base::BindOnce(&PasswordStore::GetAllLoginsImpl, this),
      base::BindOnce(&PasswordStore::InjectAffiliationAndBrandingInformation,
                     this));
}

void PasswordStore::ReportMetrics(const std::string& sync_username,
                                  bool custom_passphrase_sync_enabled,
                                  bool is_under_advanced_protection) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (background_task_runner_) {
    base::OnceClosure task = base::BindOnce(
        &PasswordStore::ReportMetricsImpl, this, sync_username,
        custom_passphrase_sync_enabled,
        BulkCheckDone(prefs_ && prefs_->HasPrefPath(
                                    prefs::kLastTimePasswordCheckCompleted)));
    background_task_runner_->PostDelayedTask(FROM_HERE, std::move(task),
                                             base::TimeDelta::FromSeconds(30));
  }

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  if (!sync_username.empty()) {
    auto hash_password_state =
        hash_password_manager_.HasPasswordHash(sync_username,
                                               /*is_gaia_password=*/true)
            ? metrics_util::IsSyncPasswordHashSaved::SAVED_VIA_LIST_PREF
            : metrics_util::IsSyncPasswordHashSaved::NOT_SAVED;
    metrics_util::LogIsSyncPasswordHashSaved(hash_password_state,
                                             is_under_advanced_protection);
  }
#endif
}

void PasswordStore::AddSiteStats(const InteractionsStats& stats) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::AddSiteStatsImpl, this, stats));
}

void PasswordStore::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(
      base::BindOnce(&PasswordStore::RemoveSiteStatsImpl, this, origin_domain));
}

void PasswordStore::GetSiteStats(const GURL& origin_domain,
                                 PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostStatsTaskAndReplyToConsumerWithResult(
      consumer,
      base::BindOnce(&PasswordStore::GetSiteStatsImpl, this, origin_domain));
}

void PasswordStore::AddCompromisedCredentials(
    const CompromisedCredentials& compromised_credentials) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto callback = base::BindOnce(&PasswordStore::AddCompromisedCredentialsImpl,
                                 this, compromised_credentials);
  ScheduleTask(base::BindOnce(
      &PasswordStore::InvokeAndNotifyAboutCompromisedPasswordsChange, this,
      std::move(callback)));
}

void PasswordStore::RemoveCompromisedCredentials(
    const std::string& signon_realm,
    const base::string16& username,
    RemoveCompromisedCredentialsReason reason) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto callback =
      base::BindOnce(&PasswordStore::RemoveCompromisedCredentialsImpl, this,
                     signon_realm, username, reason);
  ScheduleTask(base::BindOnce(
      &PasswordStore::InvokeAndNotifyAboutCompromisedPasswordsChange, this,
      std::move(callback)));
}

void PasswordStore::RemoveCompromisedCredentialsByCompromiseType(
    const std::string& signon_realm,
    const base::string16& username,
    const CompromiseType& compromise_type,
    RemoveCompromisedCredentialsReason reason) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto callback = base::BindOnce(
      &PasswordStore::RemoveCompromisedCredentialsByCompromiseTypeImpl, this,
      signon_realm, username, compromise_type, reason);
  ScheduleTask(base::BindOnce(
      &PasswordStore::InvokeAndNotifyAboutCompromisedPasswordsChange, this,
      std::move(callback)));
}

void PasswordStore::GetAllCompromisedCredentials(
    CompromisedCredentialsConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostCompromisedCredentialsTaskAndReplyToConsumerWithResult(
      consumer,
      base::BindOnce(&PasswordStore::GetAllCompromisedCredentialsImpl, this));
}

void PasswordStore::GetMatchingCompromisedCredentials(
    const std::string& signon_realm,
    CompromisedCredentialsConsumer* consumer) {
  if (affiliated_match_helper_) {
    FormDigest form(PasswordForm::Scheme::kHtml, signon_realm,
                    GURL(signon_realm));
    affiliated_match_helper_->GetAffiliatedAndroidRealms(
        form,
        base::BindOnce(&PasswordStore::ScheduleGetCompromisedWithAffiliations,
                       this, consumer->GetWeakPtr(), signon_realm));
  } else {
    PostCompromisedCredentialsTaskAndReplyToConsumerWithResult(
        consumer,
        base::BindOnce(&PasswordStore::GetMatchingCompromisedCredentialsImpl,
                       this, signon_realm));
  }
}

void PasswordStore::RemoveCompromisedCredentialsByUrlAndTime(
    base::RepeatingCallback<bool(const GURL&)> url_filter,
    base::Time remove_begin,
    base::Time remove_end,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto callback = base::BindOnce(
      &PasswordStore::RemoveCompromisedCredentialsByUrlAndTimeInternal, this,
      std::move(url_filter), remove_begin, remove_end, std::move(completion));

  ScheduleTask(base::BindOnce(
      &PasswordStore::InvokeAndNotifyAboutCompromisedPasswordsChange, this,
      std::move(callback)));
}

void PasswordStore::AddFieldInfo(const FieldInfo& field_info) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(
      base::BindOnce(&PasswordStore::AddFieldInfoImpl, this, field_info));
}

void PasswordStore::GetAllFieldInfo(PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto get_all_field_info_task =
      base::BindOnce(&PasswordStore::GetAllFieldInfoImpl, this);
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      std::move(get_all_field_info_task),
      base::BindOnce(&PasswordStoreConsumer::OnGetAllFieldInfo,
                     consumer->GetWeakPtr()));
}

void PasswordStore::RemoveFieldInfoByTime(base::Time remove_begin,
                                          base::Time remove_end,
                                          base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::RemoveFieldInfoByTimeInternal,
                              this, remove_begin, remove_end,
                              std::move(completion)));
}

void PasswordStore::ClearStore(base::OnceCallback<void(bool)> completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::ClearStoreInternal, this,
                              std::move(completion)));
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void PasswordStore::AddDatabaseCompromisedCredentialsObserver(
    DatabaseCompromisedCredentialsObserver* observer) {
  compromised_credentials_observers_->AddObserver(observer);
}

void PasswordStore::RemoveDatabaseCompromisedCredentialsObserver(
    DatabaseCompromisedCredentialsObserver* observer) {
  compromised_credentials_observers_->RemoveObserver(observer);
}

bool PasswordStore::ScheduleTask(base::OnceClosure task) {
  return background_task_runner_ &&
         background_task_runner_->PostTask(FROM_HERE, std::move(task));
}

bool PasswordStore::IsAbleToSavePasswords() const {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  return init_status_ == InitStatus::kSuccess;
}

void PasswordStore::ShutdownOnUIThread() {
  ScheduleTask(
      base::BindOnce(&PasswordStore::DestroyOnBackgroundSequence, this));
  // The AffiliationService must be destroyed from the main sequence.
  affiliated_match_helper_.reset();
  shutdown_called_ = true;
#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  if (notifier_)
    notifier_->UnsubscribeFromSigninEvents();
#endif
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStore::CreateSyncControllerDelegate() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // Note that a callback is bound for
  // GetSyncControllerDelegateOnBackgroundSequence() because this getter itself
  // must also run in the backend sequence, and the proxy object below will take
  // care of that.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      background_task_runner_,
      base::BindRepeating(
          &PasswordStore::GetSyncControllerDelegateOnBackgroundSequence,
          base::Unretained(this)));
}

void PasswordStore::SetUnsyncedCredentialsDeletionNotifier(
    std::unique_ptr<PasswordStore::UnsyncedCredentialsDeletionNotifier>
        notifier) {
  DCHECK(!deletion_notifier_);
  DCHECK(notifier);
  deletion_notifier_ = std::move(notifier);
}

void PasswordStore::SetSyncTaskTimeoutForTest(base::TimeDelta timeout) {
  sync_task_timeout_ = timeout;
}

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
void PasswordStore::CheckReuse(const base::string16& input,
                               const std::string& domain,
                               PasswordReuseDetectorConsumer* consumer) {
  ScheduleTask(base::BindOnce(&PasswordStore::CheckReuseImpl, this,
                              std::make_unique<CheckReuseRequest>(consumer),
                              input, domain));
}
#endif

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
void PasswordStore::PreparePasswordHashData(const std::string& sync_username,
                                            const bool is_signed_in) {
  SchedulePasswordHashUpdate(/*should_log_metrics=*/true,
                             !sync_username.empty(), is_signed_in);
  ScheduleEnterprisePasswordURLUpdate();
}

void PasswordStore::SaveGaiaPasswordHash(const std::string& username,
                                         const base::string16& password,
                                         bool is_primary_account,
                                         GaiaPasswordHashChange event) {
  SaveProtectedPasswordHash(username, password, is_primary_account,
                            /*is_gaia_password=*/true, event);
}

void PasswordStore::SaveEnterprisePasswordHash(const std::string& username,
                                               const base::string16& password) {
  SaveProtectedPasswordHash(
      username, password, /*is_primary_account=*/false,
      /*is_gaia_password=*/false,
      GaiaPasswordHashChange::NON_GAIA_ENTERPRISE_PASSWORD_CHANGE);
}

void PasswordStore::SaveProtectedPasswordHash(const std::string& username,
                                              const base::string16& password,
                                              bool is_primary_account,
                                              bool is_gaia_password,
                                              GaiaPasswordHashChange event) {
  if (hash_password_manager_.SavePasswordHash(username, password,
                                              is_gaia_password)) {
    if (is_gaia_password) {
      metrics_util::LogGaiaPasswordHashChange(event, is_primary_account);
    }
    // This method is not being called on startup so it shouldn't log metrics.
    // |is_signed_in| is only used when |should_log_metrics| is true so
    // it doesn't matter what the value is here.
    SchedulePasswordHashUpdate(/*should_log_metrics=*/false, is_primary_account,
                               /*is_signed_in=*/false);
  }
}

void PasswordStore::SaveSyncPasswordHash(
    const PasswordHashData& sync_password_data,
    GaiaPasswordHashChange event) {
  if (hash_password_manager_.SavePasswordHash(sync_password_data)) {
    metrics_util::LogGaiaPasswordHashChange(event,
                                            /*is_sync_password=*/true);
    SchedulePasswordHashUpdate(/*should_log_metrics=*/false,
                               /*does_primary_account_exists=*/false,
                               /*is_signed_in=*/false);
  }
}

void PasswordStore::ClearGaiaPasswordHash(const std::string& username) {
  hash_password_manager_.ClearSavedPasswordHash(username,
                                                /*is_gaia_password=*/true);
  ScheduleTask(base::BindOnce(&PasswordStore::ClearGaiaPasswordHashImpl, this,
                              username));
}

void PasswordStore::ClearAllGaiaPasswordHash() {
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ true);
  ScheduleTask(
      base::BindOnce(&PasswordStore::ClearAllGaiaPasswordHashImpl, this));
}

void PasswordStore::ClearAllEnterprisePasswordHash() {
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ false);
  ScheduleTask(
      base::BindOnce(&PasswordStore::ClearAllEnterprisePasswordHashImpl, this));
}

void PasswordStore::ClearAllNonGmailPasswordHash() {
  hash_password_manager_.ClearAllNonGmailPasswordHash();
  ScheduleTask(
      base::BindOnce(&PasswordStore::ClearAllNonGmailPasswordHashImpl, this));
}

std::unique_ptr<StateSubscription>
PasswordStore::RegisterStateCallbackOnHashPasswordManager(
    const base::RepeatingCallback<void(const std::string& username)>&
        callback) {
  return hash_password_manager_.RegisterStateCallback(callback);
}

void PasswordStore::SetPasswordStoreSigninNotifier(
    std::unique_ptr<PasswordStoreSigninNotifier> notifier) {
  DCHECK(!notifier_);
  DCHECK(notifier);
  notifier_ = std::move(notifier);
  notifier_->SubscribeToSigninEvents(this);
}

void PasswordStore::SchedulePasswordHashUpdate(bool should_log_metrics,
                                               bool does_primary_account_exists,
                                               bool is_signed_in) {
  ScheduleTask(base::BindOnce(
      &PasswordStore::SaveProtectedPasswordHashImpl, this,
      hash_password_manager_.RetrieveAllPasswordHashes(), should_log_metrics,
      does_primary_account_exists, is_signed_in));
}

void PasswordStore::ScheduleEnterprisePasswordURLUpdate() {
  std::vector<GURL> enterprise_login_urls;
  safe_browsing::GetPasswordProtectionLoginURLsPref(*prefs_,
                                                    &enterprise_login_urls);
  GURL enterprise_change_password_url =
      safe_browsing::GetPasswordProtectionChangePasswordURLPref(*prefs_);

  ScheduleTask(base::BindOnce(&PasswordStore::SaveEnterprisePasswordURLs, this,
                              std::move(enterprise_login_urls),
                              std::move(enterprise_change_password_url)));
}

#endif

PasswordStore::~PasswordStore() {
  DCHECK(shutdown_called_);
}

scoped_refptr<base::SequencedTaskRunner>
PasswordStore::CreateBackgroundTaskRunner() const {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

bool PasswordStore::InitOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  sync_bridge_ = base::WrapUnique(new PasswordSyncBridge(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::PASSWORDS, base::DoNothing()),
      /*password_store_sync=*/this, sync_enabled_or_disabled_cb_));

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  reuse_detector_ = new PasswordReuseDetector;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     reuse_detector_->GetWeakPtr(), base::RetainedRef(this),
                     GetAutofillableLoginsImpl()));
#endif
  return true;
}

PasswordStoreChangeList PasswordStore::AddLoginSync(const PasswordForm& form,
                                                    AddLoginError* error) {
  // There is no good way to check if the password is actually up to date, or
  // at least to check if it was actually changed. Assume it is.
  if (AffiliatedMatchHelper::IsValidAndroidCredential(
          PasswordStore::FormDigest(form)))
    ScheduleFindAndUpdateAffiliatedWebLogins(form);
  return AddLoginImpl(form, error);
}

PasswordStoreChangeList PasswordStore::UpdateLoginSync(
    const PasswordForm& form,
    UpdateLoginError* error) {
  if (AffiliatedMatchHelper::IsValidAndroidCredential(
          PasswordStore::FormDigest(form))) {
    // Ideally, a |form| would not be updated in any way unless it was ensured
    // that it, as a whole, can be used for a successful login. This, sadly, can
    // not be guaranteed. It might be that |form| just contains updates to some
    // meta-attribute, while it still has an out-of-date password. If such a
    // password were to be propagated to affiliated credentials in that case, it
    // may very well overwrite the actual, up-to-date password. Try to mitigate
    // this risk by ignoring updates unless they actually update the password.
    std::unique_ptr<PasswordForm> old_form(GetLoginImpl(form));
    if (old_form && form.password_value != old_form->password_value)
      ScheduleFindAndUpdateAffiliatedWebLogins(form);
  }
  return UpdateLoginImpl(form, error);
}

PasswordStoreChangeList PasswordStore::RemoveLoginSync(
    const PasswordForm& form) {
  return RemoveLoginImpl(form);
}

void PasswordStore::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!changes.empty()) {
    observers_->Notify(FROM_HERE, &Observer::OnLoginsChangedIn,
                       base::RetainedRef(this), changes);
    if (sync_bridge_)
      sync_bridge_->ActOnPasswordStoreChanges(changes);

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
    if (reuse_detector_)
      reuse_detector_->OnLoginsChanged(changes);
#endif
    ProcessLoginsChanged(
        changes,
        base::BindRepeating(
            [](scoped_refptr<PasswordStore> store,
               const std::string& signon_realm, const base::string16& username,
               RemoveCompromisedCredentialsReason reason) {
              auto callback = base::BindOnce(
                  &PasswordStore::RemoveCompromisedCredentialsImpl, store,
                  signon_realm, username, reason);
              store->InvokeAndNotifyAboutCompromisedPasswordsChange(
                  std::move(callback));
            },
            scoped_refptr<PasswordStore>(this)));
  }
}

void PasswordStore::NotifyDeletionsHaveSynced(bool success) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  // Either all deletions have been committed to the Sync server, or Sync is
  // telling us that it won't commit them (because Sync was turned off
  // permanently). In either case, run the corresponding callbacks now (on the
  // main task runner).
  DCHECK(!success || !GetMetadataStore()->HasUnsyncedDeletions());
  if (!deletions_have_synced_callbacks_.empty()) {
    base::UmaHistogramBoolean(
        "PasswordManager.PasswordStoreDeletionsHaveSynced", success);
  }
  for (auto& callback : deletions_have_synced_callbacks_) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), success));
  }
  deletions_have_synced_timeout_.Cancel();
  deletions_have_synced_callbacks_.clear();
}

void PasswordStore::InvokeAndNotifyAboutCompromisedPasswordsChange(
    base::OnceCallback<bool()> callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (std::move(callback).Run()) {
    compromised_credentials_observers_->Notify(
        FROM_HERE,
        &DatabaseCompromisedCredentialsObserver::
            OnCompromisedCredentialsChangedIn,
        base::RetainedRef(this));
  }
}

void PasswordStore::NotifyUnsyncedCredentialsWillBeDeleted(
    std::vector<PasswordForm> unsynced_credentials) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsAccountStore());
  // |deletion_notifier_| only gets set for desktop.
  if (deletion_notifier_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PasswordStore::UnsyncedCredentialsDeletionNotifier::Notify,
            deletion_notifier_->GetWeakPtr(), std::move(unsynced_credentials)));
  }
}

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
void PasswordStore::CheckReuseImpl(std::unique_ptr<CheckReuseRequest> request,
                                   const base::string16& input,
                                   const std::string& domain) {
  if (reuse_detector_) {
    reuse_detector_->CheckReuse(input, domain, request.get());
  } else {
    request->OnReuseCheckDone(false, 0, base::nullopt, {}, 0);
  }
}

void PasswordStore::SaveProtectedPasswordHashImpl(
    PasswordHashDataList protected_password_data_list,
    bool should_log_metrics,
    bool does_primary_account_exists,
    bool is_signed_in) {
  if (!reuse_detector_ || !protected_password_data_list.has_value())
    return;
  TRACE_EVENT0("passwords", "PasswordStore::SaveProtectedPasswordHashImpl");

  std::vector<PasswordHashData> gaia_password_hash_list;
  std::vector<PasswordHashData> enterprise_password_hash_list;
  for (PasswordHashData& password_hash : *protected_password_data_list) {
    if (password_hash.is_gaia_password)
      gaia_password_hash_list.push_back(std::move(password_hash));
    else
      enterprise_password_hash_list.push_back(std::move(password_hash));
  }

  if (should_log_metrics) {
    metrics_util::LogProtectedPasswordHashCounts(
        gaia_password_hash_list.size(), enterprise_password_hash_list.size(),
        does_primary_account_exists, is_signed_in);
  }
  reuse_detector_->UseGaiaPasswordHash(std::move(gaia_password_hash_list));
  reuse_detector_->UseNonGaiaEnterprisePasswordHash(
      std::move(enterprise_password_hash_list));
}

void PasswordStore::SaveEnterprisePasswordURLs(
    const std::vector<GURL>& enterprise_login_urls,
    const GURL& enterprise_change_password_url) {
  if (!reuse_detector_)
    return;
  TRACE_EVENT0("passwords", "PasswordStore::SaveEnterprisePasswordURLs");
  reuse_detector_->UseEnterprisePasswordURLs(std::move(enterprise_login_urls),
                                             enterprise_change_password_url);
}

void PasswordStore::ClearGaiaPasswordHashImpl(const std::string& username) {
  TRACE_EVENT0("passwords", "PasswordStore::ClearGaiaPasswordHashImpl");
  if (reuse_detector_)
    reuse_detector_->ClearGaiaPasswordHash(username);
}

void PasswordStore::ClearAllGaiaPasswordHashImpl() {
  TRACE_EVENT0("passwords", "PasswordStore::ClearAllGaiaPasswordHashImpl");
  if (reuse_detector_)
    reuse_detector_->ClearAllGaiaPasswordHash();
}

void PasswordStore::ClearAllEnterprisePasswordHashImpl() {
  TRACE_EVENT0("passwords",
               "PasswordStore::ClearAllEnterprisePasswordHashImpl");
  if (reuse_detector_)
    reuse_detector_->ClearAllEnterprisePasswordHash();
}

void PasswordStore::ClearAllNonGmailPasswordHashImpl() {
  TRACE_EVENT0("passwords", "PasswordStore::ClearAllNonGmailPasswordHashImpl");
  if (reuse_detector_)
    reuse_detector_->ClearAllNonGmailPasswordHash();
}

#endif

void PasswordStore::OnInitCompleted(bool success) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  init_status_ = success ? InitStatus::kSuccess : InitStatus::kFailure;

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.PasswordStoreInitResult", success);
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "passwords", "PasswordStore::InitOnBackgroundSequence", this);
}

void PasswordStore::PostLoginsTaskAndReplyToConsumerWithResult(
    PasswordStoreConsumer* consumer,
    LoginsTask task) {
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this)));
}

void PasswordStore::PostLoginsTaskAndReplyToConsumerWithProcessedResult(
    const char* trace_name,
    PasswordStoreConsumer* consumer,
    LoginsTask task,
    LoginsResultProcessor processor) {
  auto call_consumer = base::BindOnce(
      CloseTraceAndCallBack, trace_name, consumer,
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this)));
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(std::move(processor), std::move(call_consumer)));
}

void PasswordStore::PostStatsTaskAndReplyToConsumerWithResult(
    PasswordStoreConsumer* consumer,
    StatsTask task) {
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(&PasswordStoreConsumer::OnGetSiteStatistics,
                     consumer->GetWeakPtr()));
}

void PasswordStore::PostCompromisedCredentialsTaskAndReplyToConsumerWithResult(
    CompromisedCredentialsConsumer* consumer,
    CompromisedCredentialsTask task) {
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(
          &CompromisedCredentialsConsumer::OnGetCompromisedCredentialsFrom,
          consumer->GetWeakPtr(), base::RetainedRef(this)));
}

void PasswordStore::AddLoginInternal(const PasswordForm& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::AddLoginInternal");
  BeginTransaction();
  PasswordStoreChangeList changes = AddLoginImpl(form);
  NotifyLoginsChanged(changes);
  // Sync metadata get updated in NotifyLoginsChanged(). Therefore,
  // CommitTransaction() must be called after NotifyLoginsChanged(), because
  // sync codebase needs to update metadata atomically together with the login
  // data.
  CommitTransaction();
}

void PasswordStore::UpdateLoginInternal(const PasswordForm& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::UpdateLoginInternal");
  BeginTransaction();
  PasswordStoreChangeList changes = UpdateLoginImpl(form);
  NotifyLoginsChanged(changes);
  // Sync metadata get updated in NotifyLoginsChanged(). Therefore,
  // CommitTransaction() must be called after NotifyLoginsChanged(), because
  // sync codebase needs to update metadata atomically together with the login
  // data.
  CommitTransaction();
}

void PasswordStore::RemoveLoginInternal(const PasswordForm& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::RemoveLoginInternal");
  BeginTransaction();
  PasswordStoreChangeList changes = RemoveLoginImpl(form);
  NotifyLoginsChanged(changes);
  // Sync metadata get updated in NotifyLoginsChanged(). Therefore,
  // CommitTransaction() must be called after NotifyLoginsChanged(), because
  // sync codebase needs to update metadata atomically together with the login
  // data.
  CommitTransaction();
}

void PasswordStore::UpdateLoginWithPrimaryKeyInternal(
    const PasswordForm& new_form,
    const PasswordForm& old_primary_key) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::UpdateLoginWithPrimaryKeyInternal");
  BeginTransaction();
  PasswordStoreChangeList all_changes = RemoveLoginImpl(old_primary_key);
  PasswordStoreChangeList changes = AddLoginImpl(new_form);
  all_changes.insert(all_changes.end(), changes.begin(), changes.end());
  NotifyLoginsChanged(all_changes);
  // Sync metadata get updated in NotifyLoginsChanged(). Therefore,
  // CommitTransaction() must be called after NotifyLoginsChanged(), because
  // sync codebase needs to update metadata atomically together with the login
  // data.
  CommitTransaction();
}

void PasswordStore::RemoveLoginsByURLAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion,
    base::OnceCallback<void(bool)> sync_completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::RemoveLoginsByURLAndTimeInternal");
  BeginTransaction();
  PasswordStoreChangeList changes =
      RemoveLoginsByURLAndTimeImpl(url_filter, delete_begin, delete_end);
  NotifyLoginsChanged(changes);
  // Sync metadata get updated in NotifyLoginsChanged(). Therefore,
  // CommitTransaction() must be called after NotifyLoginsChanged(), because
  // sync codebase needs to update metadata atomically together with the login
  // data.
  CommitTransaction();

  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));

  if (sync_completion) {
    deletions_have_synced_callbacks_.push_back(std::move(sync_completion));
    // Start a timeout for sync, or restart it if it was already running.
    deletions_have_synced_timeout_.Reset(base::BindRepeating(
        &PasswordStore::NotifyDeletionsHaveSynced, this, /*success=*/false));
    background_task_runner_->PostDelayedTask(
        FROM_HERE, deletions_have_synced_timeout_.callback(),
        sync_task_timeout_);

    // Do an immediate check for the case where there are already no unsynced
    // deletions.
    if (!GetMetadataStore()->HasUnsyncedDeletions())
      NotifyDeletionsHaveSynced(/*success=*/true);
  }
}

void PasswordStore::RemoveLoginsCreatedBetweenInternal(
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords",
               "PasswordStore::RemoveLoginsCreatedBetweenInternal");
  BeginTransaction();
  PasswordStoreChangeList changes =
      RemoveLoginsCreatedBetweenImpl(delete_begin, delete_end);
  NotifyLoginsChanged(changes);
  // Sync metadata get updated in NotifyLoginsChanged(). Therefore,
  // CommitTransaction() must be called after NotifyLoginsChanged(), because
  // sync codebase needs to update metadata atomically together with the login
  // data.
  CommitTransaction();
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

void PasswordStore::RemoveStatisticsByOriginAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords",
               "PasswordStore::RemoveStatisticsByOriginAndTimeInternal");
  RemoveStatisticsByOriginAndTimeImpl(origin_filter, delete_begin, delete_end);
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

void PasswordStore::DisableAutoSignInForOriginsInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords",
               "PasswordStore::DisableAutoSignInForOriginsInternal");
  DisableAutoSignInForOriginsImpl(origin_filter);
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

void PasswordStore::UnblacklistInternal(
    const PasswordStore::FormDigest& form_digest,
    base::OnceClosure completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::UnblacklistInternal");

  std::vector<std::unique_ptr<PasswordForm>> all_matches =
      GetLoginsImpl(form_digest);
  for (auto& form : all_matches) {
    // Ignore PSL matches for blocked entries.
    if (form->blocked_by_user && !form->is_public_suffix_match)
      RemoveLoginInternal(*form);
  }
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

bool PasswordStore::RemoveCompromisedCredentialsByUrlAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end,
    base::OnceClosure completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  bool result = RemoveCompromisedCredentialsByUrlAndTimeImpl(
      url_filter, remove_begin, remove_end);
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
  return result;
}

void PasswordStore::RemoveFieldInfoByTimeInternal(
    base::Time remove_begin,
    base::Time remove_end,
    base::OnceClosure completion) {
  RemoveFieldInfoByTimeImpl(remove_begin, remove_end);
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

void PasswordStore::ClearStoreInternal(
    base::OnceCallback<void(bool)> completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  bool should_clear = !IsEmpty();
  if (should_clear)
    DeleteAndRecreateDatabaseFile();
  if (completion) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion), should_clear));
  }
}

std::vector<std::unique_ptr<PasswordForm>> PasswordStore::GetLoginsImpl(
    const FormDigest& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return FillMatchingLogins(form);
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStore::GetLoginsByPasswordImpl(
    const base::string16& plain_text_password) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::GetLoginsByPasswordImpl");
  return FillMatchingLoginsByPassword(plain_text_password);
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStore::GetAutofillableLoginsImpl() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::GetAutofillableLoginsImpl");
  std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
  if (!FillAutofillableLogins(&obtained_forms))
    obtained_forms.clear();
  return obtained_forms;
}

std::vector<std::unique_ptr<PasswordForm>> PasswordStore::GetAllLoginsImpl() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::GetAllLoginsImpl");
  std::vector<std::unique_ptr<PasswordForm>> results;
  for (auto fill_logins : {&PasswordStore::FillAutofillableLogins,
                           &PasswordStore::FillBlacklistLogins}) {
    std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
    if ((this->*fill_logins)(&obtained_forms)) {
      results.insert(results.end(),
                     std::make_move_iterator(obtained_forms.begin()),
                     std::make_move_iterator(obtained_forms.end()));
    }
  }

  return results;
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStore::GetLoginsWithAffiliationsImpl(
    const FormDigest& form,
    const std::vector<std::string>& additional_android_realms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<PasswordForm>> results(FillMatchingLogins(form));
  for (const std::string& realm : additional_android_realms) {
    std::vector<std::unique_ptr<PasswordForm>> more_results(
        FillMatchingLogins({PasswordForm::Scheme::kHtml, realm, GURL()}));
    for (auto& result : more_results)
      result->is_affiliation_based_match = true;
    password_manager_util::TrimUsernameOnlyCredentials(&more_results);
    results.insert(results.end(), std::make_move_iterator(more_results.begin()),
                   std::make_move_iterator(more_results.end()));
  }

  return results;
}

std::vector<CompromisedCredentials>
PasswordStore::GetCompromisedWithAffiliationsImpl(
    const std::string& signon_realm,
    const std::vector<std::string>& additional_android_realms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<CompromisedCredentials> results(
      GetMatchingCompromisedCredentialsImpl(signon_realm));
  for (const std::string& realm : additional_android_realms) {
    std::vector<CompromisedCredentials> more_results(
        GetMatchingCompromisedCredentialsImpl(realm));
    results.insert(results.end(), std::make_move_iterator(more_results.begin()),
                   std::make_move_iterator(more_results.end()));
  }

  return results;
}

void PasswordStore::InjectAffiliationAndBrandingInformation(
    LoginsReply callback,
    LoginsResult forms) {
  if (affiliated_match_helper_) {
    affiliated_match_helper_->InjectAffiliationAndBrandingInformation(
        std::move(forms), AndroidAffiliationService::StrategyOnCacheMiss::FAIL,
        std::move(callback));
  } else {
    std::move(callback).Run(std::move(forms));
  }
}

void PasswordStore::ScheduleGetFilteredLoginsWithAffiliations(
    base::WeakPtr<PasswordStoreConsumer> consumer,
    const PasswordStore::FormDigest& form,
    base::Time cutoff,
    const std::vector<std::string>& additional_android_realms) {
  if (consumer) {
    PostLoginsTaskAndReplyToConsumerWithProcessedResult(
        "PasswordStore::GetLogins", consumer.get(),
        base::BindOnce(&PasswordStore::GetLoginsWithAffiliationsImpl, this,
                       form, additional_android_realms),
        base::BindOnce(FilterLogins, cutoff));
  }
}

void PasswordStore::ScheduleGetCompromisedWithAffiliations(
    base::WeakPtr<CompromisedCredentialsConsumer> consumer,
    const std::string& signon_realm,
    const std::vector<std::string>& additional_android_realms) {
  if (consumer) {
    PostCompromisedCredentialsTaskAndReplyToConsumerWithResult(
        consumer.get(),
        base::BindOnce(&PasswordStore::GetCompromisedWithAffiliationsImpl, this,
                       signon_realm, additional_android_realms));
  }
}

std::unique_ptr<PasswordForm> PasswordStore::GetLoginImpl(
    const PasswordForm& primary_key) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<PasswordForm>> candidates(
      FillMatchingLogins(FormDigest(primary_key)));
  for (auto& candidate : candidates) {
    if (ArePasswordFormUniqueKeysEqual(*candidate, primary_key) &&
        !candidate->is_public_suffix_match) {
      return std::move(candidate);
    }
  }
  return nullptr;
}

void PasswordStore::FindAndUpdateAffiliatedWebLogins(
    const PasswordForm& added_or_updated_android_form) {
  if (!affiliated_match_helper_)
    return;
  affiliated_match_helper_->GetAffiliatedWebRealms(
      PasswordStore::FormDigest(added_or_updated_android_form),
      base::BindOnce(&PasswordStore::ScheduleUpdateAffiliatedWebLoginsImpl,
                     this, added_or_updated_android_form));
}

void PasswordStore::ScheduleFindAndUpdateAffiliatedWebLogins(
    const PasswordForm& added_or_updated_android_form) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStore::FindAndUpdateAffiliatedWebLogins, this,
                     added_or_updated_android_form));
}

void PasswordStore::UpdateAffiliatedWebLoginsImpl(
    const PasswordForm& updated_android_form,
    const std::vector<std::string>& affiliated_web_realms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  BeginTransaction();
  PasswordStoreChangeList all_changes;
  for (const std::string& affiliated_web_realm : affiliated_web_realms) {
    std::vector<std::unique_ptr<PasswordForm>> web_logins(FillMatchingLogins(
        {PasswordForm::Scheme::kHtml, affiliated_web_realm, GURL()}));
    for (auto& web_login : web_logins) {
      // Do not update HTTP logins, logins saved under insecure conditions, and
      // non-HTML login forms; PSL matches; logins with a different username;
      // and logins with the same password (to avoid generating no-op updates).
      if (!AffiliatedMatchHelper::IsValidWebCredential(
              FormDigest(*web_login)) ||
          web_login->is_public_suffix_match ||
          web_login->username_value != updated_android_form.username_value ||
          web_login->password_value == updated_android_form.password_value)
        continue;

      // If the |web_login| was updated in the same or a later chunk of Sync
      // changes, assume that it is more recent and do not update it. Note that
      // this check is far from perfect conflict resolution and mostly prevents
      // long-dormant Sync clients doing damage when they wake up in the face
      // of the following list of changes:
      //
      //   Time   Source     Change
      //   ====   ======     ======
      //   #1     Android    android_login.password_value = "A"
      //   #2     Client A   web_login.password_value = "A" (propagation)
      //   #3     Client A   web_login.password_value = "B" (manual overwrite)
      //
      // When long-dormant Sync client B wakes up, it will only get a distilled
      // subset of not-yet-obsoleted changes {1, 3}. In this case, client B must
      // not propagate password "A" to |web_login|. This is prevented as change
      // #3 will arrive either in the same/later chunk of sync changes, so the
      // |date_synced| of |web_login| value will be greater or equal.
      //
      // Note that this solution has several shortcomings:
      //
      //   (1) It will not prevent local changes to |web_login| from being
      //       overwritten if they were made shortly after start-up, before
      //       Sync changes are applied. This should be tolerable.
      //
      //   (2) It assumes that all Sync clients are fully capable of propagating
      //       changes to web credentials on their own. If client C runs an
      //       older version of Chrome and updates the password for |web_login|
      //       around the time when the |android_login| is updated, the updated
      //       password will not be propagated by client B to |web_login| when
      //       it wakes up, regardless of the temporal order of the original
      //       changes, as client B will see both credentials having the same
      //       |data_synced|.
      //
      //   (2a) Above could be mitigated by looking not only at |data_synced|,
      //        but also at the actual order of Sync changes.
      //
      //   (2b) However, (2a) is still not workable, as a Sync change is made
      //        when any attribute of the credential is updated, not only the
      //        password. Hence it is not possible for client B to distinguish
      //        between two following two event orders:
      //
      //    #1     Android    android_login.password_value = "A"
      //    #2     Client C   web_login.password_value = "B" (manual overwrite)
      //    #3     Android    android_login.random_attribute = "..."
      //
      //    #1     Client C   web_login.password_value = "B" (manual overwrite)
      //    #2     Android    android_login.password_value = "A"
      //
      //        And so it must assume that it is unsafe to update |web_login|.
      if (web_login->date_synced >= updated_android_form.date_synced)
        continue;

      web_login->password_value = updated_android_form.password_value;

      PasswordStoreChangeList changes = UpdateLoginImpl(*web_login);
      all_changes.insert(all_changes.end(), changes.begin(), changes.end());
    }
  }
  NotifyLoginsChanged(all_changes);
  // Sync metadata get updated in NotifyLoginsChanged(). Therefore,
  // CommitTransaction() must be called after NotifyLoginsChanged(), because
  // sync codebase needs to update metadata atomically together with the login
  // data.
  CommitTransaction();
}

void PasswordStore::ScheduleUpdateAffiliatedWebLoginsImpl(
    const PasswordForm& updated_android_form,
    const std::vector<std::string>& affiliated_web_realms) {
  ScheduleTask(base::BindOnce(&PasswordStore::UpdateAffiliatedWebLoginsImpl,
                              this, updated_android_form,
                              affiliated_web_realms));
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasswordStore::GetSyncControllerDelegateOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(sync_bridge_);
  return sync_bridge_->change_processor()->GetControllerDelegate();
}

void PasswordStore::DestroyOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  sync_bridge_.reset();

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  delete reuse_detector_;
  reuse_detector_ = nullptr;
#endif
}

std::ostream& operator<<(std::ostream& os,
                         const PasswordStore::FormDigest& digest) {
  return os << "FormDigest(scheme: " << digest.scheme
            << ", signon_realm: " << digest.signon_realm
            << ", url: " << digest.url << ")";
}

}  // namespace password_manager
