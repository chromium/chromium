// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/browser/password_store/smart_bubble_stats_store.h"
#include "components/password_manager/core/common/password_manager_features.h"

using Logger = autofill::SavePasswordProgressLogger;
using password_manager_util::GetMatchType;

namespace password_manager {

namespace {

// Given |non_federated| matches where all matches with the |scheme| are in the
// beginning of the vector, returns a span with those matches.
// |Form| is either a const PasswordForm or PasswordForm depending on the
// context.
template <typename Form>
base::span<Form> NonFederatedSameSchemeMatches(base::span<Form> non_federated,
                                               PasswordForm::Scheme scheme) {
  const auto same_scheme_count = static_cast<size_t>(
      std::ranges::count(non_federated, scheme, &Form::scheme));
  return non_federated.first(same_scheme_count);
}
}  // namespace

FormFetcherImpl::FormFetcherImpl(PasswordFormDigest form_digest,
                                 PasswordManagerClient* client,
                                 bool should_migrate_http_passwords)
    : form_digest_(std::move(form_digest)),
      client_(client),
      should_migrate_http_passwords_(should_migrate_http_passwords &&
                                     form_digest_.scheme ==
                                         PasswordForm::Scheme::kHtml) {}

FormFetcherImpl::~FormFetcherImpl() = default;

void FormFetcherImpl::AddConsumer(FormFetcher::Consumer* consumer) {
  DCHECK(consumer);
  consumers_.AddObserver(consumer);
  if (state_ == State::NOT_WAITING) {
    // Notify the consumer immediately, but do it async so that it's in line
    // with the regular form fetcher behavior.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FormFetcherImpl::NotifyConsumer,
                                  weak_ptr_factory_.GetWeakPtr(), consumer));
  }
}

void FormFetcherImpl::RemoveConsumer(FormFetcher::Consumer* consumer) {
  DCHECK(consumers_.HasObserver(consumer));
  consumers_.RemoveObserver(consumer);
}

void FormFetcherImpl::Fetch() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetCurrentLogManager());
    logger->LogMessage(Logger::STRING_FETCH_METHOD);
    logger->LogNumber(Logger::STRING_FORM_FETCHER_STATE,
                      static_cast<int>(state_));
  }

  if (state_ == State::WAITING) {
    // There is currently a password store query in progress, need to re-fetch
    // store results later.
    need_to_refetch_ = true;
    return;
  }

  state_ = State::WAITING;

  // Issue a fetch from the profile store and, if it exists, also from the
  // account store.
  // Set up |wait_counter_| *before* triggering any of the fetches. This ensures
  // that things work correctly (i.e. we don't notify of completion too early)
  // even if the fetches return synchronously (which is the case in tests).
  wait_counter_++;
  // Clears the flag since it will be outdated after this fetch is finished.
  grouped_credentials_form_type_ = std::nullopt;
  PasswordStoreInterface* profile_password_store =
      client_->GetProfilePasswordStore();
  if (!profile_password_store) {
    if (logger) {
      logger->LogMessage(Logger::STRING_NO_STORE);
    }

    std::vector<StoredCredential> results;
    AggregatePasswordStoreResults(std::move(results));
    return;
  }

  PasswordStoreInterface* account_password_store =
      client_->GetAccountPasswordStore();
  if (account_password_store) {
    wait_counter_++;
  }

  profile_password_store->GetLogins(form_digest_,
                                    weak_ptr_factory_.GetWeakPtr());
  if (account_password_store) {
    account_password_store->GetLogins(form_digest_,
                                      weak_ptr_factory_.GetWeakPtr());
  }

// The statistics isn't needed on mobile, only on desktop. Let's save some
// processor cycles.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // The statistics is needed for the "Save password?" bubble.
  password_manager::SmartBubbleStatsStore* stats_store =
      profile_password_store->GetSmartBubbleStatsStore();
  // `stats_store` can be null in tests.
  if (stats_store) {
    stats_store->GetSiteStats(form_digest_.url.DeprecatedGetOriginAsURL(),
                              weak_ptr_factory_.GetWeakPtr());
  }
#endif
}

FormFetcherImpl::State FormFetcherImpl::GetState() const {
  return state_;
}

const std::vector<InteractionsStats>& FormFetcherImpl::GetInteractionsStats()
    const {
  return interactions_stats_;
}

base::span<const StoredCredential> FormFetcherImpl::GetInsecureCredentials()
    const {
  return insecure_credentials_;
}

base::span<const StoredCredential> FormFetcherImpl::GetNonFederatedMatches()
    const {
  return non_federated_;
}

base::span<const StoredCredential> FormFetcherImpl::GetFederatedMatches()
    const {
  return federated_;
}

bool FormFetcherImpl::IsBlocklisted() const {
  // TODO(crbug.com/470332074): Verify whether this should check for
  // "enabled" instead of "active".
  if (client_->GetPasswordFeatureManager()->IsAccountStorageActive()) {
    return is_blocklisted_in_account_store_;
  }
  return is_blocklisted_in_profile_store_;
}

bool FormFetcherImpl::IsMovingBlocked(const signin::GaiaIdHash& destination,
                                      const std::u16string& username) const {
  for (const std::vector<StoredCredential>* matches_vector :
       {&federated_, &non_federated_}) {
    for (const auto& form : *matches_vector) {
      // Only local entries can be moved to the account store (though
      // account store matches should never have |moving_blocked_for_list|
      // entries anyway).
      if (form.IsUsingAccountStore()) {
        continue;
      }
      // Ignore non-exact matches for blocking moving. PLS, affiliated and
      // grouped matches are ignored.
      if (GetMatchType(form) !=
          password_manager_util::GetLoginMatchType::kExact) {
        continue;
      }
      if (form.username_value != username) {
        continue;
      }
      if (std::ranges::contains(form.moving_blocked_for_list, destination)) {
        return true;
      }
    }
  }
  return false;
}

base::span<const StoredCredential> FormFetcherImpl::GetAllRelevantMatches()
    const {
  return NonFederatedSameSchemeMatches(base::span(non_federated_),
                                       form_digest_.scheme);
}

base::span<const StoredCredential> FormFetcherImpl::GetBestMatches() const {
  return best_matches_;
}

const StoredCredential* FormFetcherImpl::GetPreferredMatch() const {
  if (best_matches_.empty()) {
    return nullptr;
  }
  return &(*best_matches_.begin());
}

std::optional<PasswordFormMetricsRecorder::MatchedFormType>
FormFetcherImpl::GetPreferredOrPotentialMatchedFormType() const {
  const StoredCredential* preferred_match = GetPreferredMatch();
  if (!preferred_match) {
    return grouped_credentials_form_type_;
  }
  switch (password_manager_util::GetMatchType(CHECK_DEREF(preferred_match))) {
    case password_manager_util::GetLoginMatchType::kExact:
      return PasswordFormMetricsRecorder::MatchedFormType::kExactMatch;
    case password_manager_util::GetLoginMatchType::kAffiliated:
      return affiliations::IsValidAndroidFacetURI(
                 CHECK_DEREF(preferred_match).signon_realm)
                 ? PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedApp
                 : PasswordFormMetricsRecorder::MatchedFormType::
                       kAffiliatedWebsites;
    case password_manager_util::GetLoginMatchType::kPSL:
      return PasswordFormMetricsRecorder::MatchedFormType::kPublicSuffixMatch;
    case password_manager_util::GetLoginMatchType::kGrouped:
      // Reaching this block implies the `FormFetched` is configured to include
      // grouped credentials in the result set.
      return affiliations::IsValidAndroidFacetURI(
                 CHECK_DEREF(preferred_match).signon_realm)
                 ? PasswordFormMetricsRecorder::MatchedFormType::kGroupedApp
                 : PasswordFormMetricsRecorder::MatchedFormType::
                       kGroupedWebsites;
  }
}

std::unique_ptr<FormFetcher> FormFetcherImpl::Clone() {
  // Create the copy without the "HTTPS migration" activated. If it was needed,
  // then it was done by |this| already.
  auto result = std::make_unique<FormFetcherImpl>(form_digest_, client_, false);

  if (state_ != State::NOT_WAITING) {
    // There are no store results to copy, trigger a Fetch on the clone instead.
    result->Fetch();
    return result;
  }

  result->non_federated_ =
      base::ToVector(non_federated_, &CloneStoredCredential);
  result->federated_ = base::ToVector(federated_, &CloneStoredCredential);
  result->is_blocklisted_in_account_store_ = is_blocklisted_in_account_store_;
  result->is_blocklisted_in_profile_store_ = is_blocklisted_in_profile_store_;
  result->best_matches_ = base::ToVector(best_matches_, &CloneStoredCredential);
  result->interactions_stats_ = interactions_stats_;
  result->insecure_credentials_ =
      base::ToVector(insecure_credentials_, &CloneStoredCredential);
  result->state_ = state_;
  result->need_to_refetch_ = need_to_refetch_;
  result->profile_store_backend_error_ = profile_store_backend_error_;
  result->account_store_backend_error_ = account_store_backend_error_;

  return result;
}

std::optional<PasswordStoreBackendError>
FormFetcherImpl::GetProfileStoreBackendError() const {
  return profile_store_backend_error_;
}

std::optional<PasswordStoreBackendError>
FormFetcherImpl::GetAccountStoreBackendError() const {
  return account_store_backend_error_;
}

void FormFetcherImpl::NotifyConsumer(FormFetcher::Consumer* consumer) {
  // If a fetch has started in the meantime, the `consumer` will be notified
  // when it finishes.
  if (state_ == State::NOT_WAITING && consumers_.HasObserver(consumer)) {
    consumer->OnFetchCompleted();
  }
}

void FormFetcherImpl::FindMatchesAndNotifyConsumers(
    std::vector<StoredCredential> results) {
  DCHECK_EQ(State::WAITING, state_);
  SplitResults(std::move(results));

  best_matches_ =
      password_manager_util::FindBestMatches(NonFederatedSameSchemeMatches(
          base::span(non_federated_), form_digest_.scheme));

  state_ = State::NOT_WAITING;
  for (auto& consumer : consumers_) {
    consumer.OnFetchCompleted();
  }
}

void FormFetcherImpl::SplitResults(std::vector<StoredCredential> forms) {
  is_blocklisted_in_profile_store_ = false;
  is_blocklisted_in_account_store_ = false;
  non_federated_.clear();
  federated_.clear();
  insecure_credentials_.clear();
  std::vector<StoredCredential> non_federated_other_schemas;

  for (auto& form : forms) {
    if (form.blocked_by_user) {
      // Ignore non-exact matches for blocklisted entries. PLS, affiliated and
      // grouped matches are ignored.
      if (password_manager_util::GetMatchType(form) ==
              password_manager_util::GetLoginMatchType::kExact &&
          form.scheme == form_digest_.scheme) {
        if (form.IsUsingAccountStore()) {
          is_blocklisted_in_account_store_ = true;
        } else {
          is_blocklisted_in_profile_store_ = true;
        }
      }
    } else {
      if (!form.password_issues.empty()) {
        insecure_credentials_.push_back(CloneStoredCredential(form));
      }
      if (form.federation_origin.IsValid()) {
        federated_.push_back(std::move(form));
      } else if (form.scheme == form_digest_.scheme) {
        non_federated_.push_back(std::move(form));
      } else {
        non_federated_other_schemas.push_back(std::move(form));
      }
    }
  }

  non_federated_.insert(
      non_federated_.end(),
      std::make_move_iterator(non_federated_other_schemas.begin()),
      std::make_move_iterator(non_federated_other_schemas.end()));
}

void FormFetcherImpl::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError results_or_error) {
  if (store == client_->GetProfilePasswordStore()) {
    profile_store_backend_error_.reset();
    if (std::holds_alternative<PasswordStoreBackendError>(results_or_error)) {
      profile_store_backend_error_ =
          std::get<PasswordStoreBackendError>(results_or_error);
    }
  } else if (store == client_->GetAccountPasswordStore()) {
    account_store_backend_error_.reset();
    if (std::holds_alternative<PasswordStoreBackendError>(results_or_error)) {
      account_store_backend_error_ =
          std::get<PasswordStoreBackendError>(results_or_error);
    }
  }

  bool has_backend_error =
      std::holds_alternative<PasswordStoreBackendError>(results_or_error);

  std::vector<StoredCredential> results =
      GetLoginsOrEmptyListOnFailure(std::move(results_or_error));
  if (filter_grouped_credentials_) {
    std::erase_if(results, [this](const auto& form) {
      if (form.match_type == PasswordForm::MatchType::kGrouped) {
        // To achieve consistency for
        // `FormFetcher::GetPreferredOrPotentialMatchFormType()`, grouped
        // website credentials are prioritized over grouped application
        // credentials if both are available.
        if (affiliations::IsValidAndroidFacetURI(form.signon_realm)) {
          // To prioritize grouped website credentials, assign
          // `grouped_credentials_form_type_` to `kGroupedApp` only if the
          // member variable was not initialized before.
          if (!grouped_credentials_form_type_) {
            grouped_credentials_form_type_ =
                PasswordFormMetricsRecorder::MatchedFormType::kGroupedApp;
          }
        } else {
          grouped_credentials_form_type_ =
              PasswordFormMetricsRecorder::MatchedFormType::kGroupedWebsites;
        }
        return true;
      }
      return false;
    });
  }

  DCHECK_EQ(State::WAITING, state_);
  DCHECK_GT(wait_counter_, 0);

  if (should_migrate_http_passwords_ && results.empty() && !has_backend_error &&
      form_digest_.url.SchemeIs(url::kHttpsScheme)) {
    http_migrators_[store] = std::make_unique<HttpPasswordStoreMigrator>(
        url::Origin::Create(form_digest_.url), store,
        client_->GetNetworkContext(), this);
    // The migrator will call us back at ProcessMigratedForms().
    return;
  }

  AggregatePasswordStoreResults(std::move(results));
}

void FormFetcherImpl::AggregatePasswordStoreResults(
    std::vector<StoredCredential> results) {
  // Store the results.
  for (auto& form : results) {
    partial_results_.push_back(std::move(form));
  }

  // If we're still awaiting more results, nothing else to do.
  if (--wait_counter_ > 0) {
    return;
  }

  if (need_to_refetch_) {
    // The received results are no longer up to date, need to re-request.
    state_ = State::NOT_WAITING;
    partial_results_.clear();
    Fetch();
    need_to_refetch_ = false;
    return;
  }

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetCurrentLogManager());
    logger.LogMessage(Logger::STRING_ON_GET_STORE_RESULTS_METHOD);
    logger.LogNumber(Logger::STRING_NUMBER_RESULTS, partial_results_.size());
  }
  FindMatchesAndNotifyConsumers(std::move(partial_results_));
}

void FormFetcherImpl::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {
  interactions_stats_ = std::move(stats);
}

void FormFetcherImpl::ProcessMigratedForms(std::vector<PasswordForm> forms) {
  // The migration from HTTP to HTTPS (within the profile store) was finished.
  // Continue processing with the migrated results.
  AggregatePasswordStoreResults(FromPasswordForms(std::move(forms)));
}

}  // namespace password_manager
