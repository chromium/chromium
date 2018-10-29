// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

// Shorten the name to spare line breaks. The code provides enough context
// already.
using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {

// Splits |store_results| into a vector of non-federated and federated matches.
// Returns the federated matches and keeps the non-federated in |store_results|.
std::vector<std::unique_ptr<PasswordForm>> SplitFederatedMatches(
    std::vector<std::unique_ptr<PasswordForm>>* store_results) {
  const auto first_federated = std::partition(
      store_results->begin(), store_results->end(),
      [](const std::unique_ptr<PasswordForm>& form) {
        return form->federation_origin.opaque();  // False means federated.
      });

  // Move out federated matches.
  std::vector<std::unique_ptr<PasswordForm>> federated_matches;
  federated_matches.resize(store_results->end() - first_federated);
  std::move(first_federated, store_results->end(), federated_matches.begin());

  store_results->erase(first_federated, store_results->end());
  return federated_matches;
}

void SplitSuppressedFormsAndAssignTo(
    const PasswordStore::FormDigest& observed_form_digest,
    std::vector<std::unique_ptr<PasswordForm>> suppressed_forms,
    std::vector<std::unique_ptr<PasswordForm>>* same_origin_https_forms,
    std::vector<std::unique_ptr<PasswordForm>>* psl_matching_forms,
    std::vector<std::unique_ptr<PasswordForm>>* same_organization_name_forms) {
  DCHECK(same_origin_https_forms);
  DCHECK(psl_matching_forms);
  DCHECK(same_organization_name_forms);
  same_origin_https_forms->clear();
  psl_matching_forms->clear();
  same_organization_name_forms->clear();
  for (auto& form : suppressed_forms) {
    switch (GetMatchResult(*form, observed_form_digest)) {
      case MatchResult::PSL_MATCH:
        psl_matching_forms->push_back(std::move(form));
        break;
      case MatchResult::NO_MATCH:
        if (form->origin.host() != observed_form_digest.origin.host()) {
          same_organization_name_forms->push_back(std::move(form));
        } else if (form->origin.SchemeIs(url::kHttpsScheme) &&
                   observed_form_digest.origin.SchemeIs(url::kHttpScheme)) {
          same_origin_https_forms->push_back(std::move(form));
        } else {
          // HTTP form suppressed on HTTPS observed page: The HTTP->HTTPS
          // migration can leave tons of such HTTP forms behind, ignore these.
        }
        break;
      case MatchResult::EXACT_MATCH:
      case MatchResult::FEDERATED_MATCH:
      case MatchResult::FEDERATED_PSL_MATCH:
        NOTREACHED() << "Suppressed match cannot be exact or federated.";
        break;
    }
  }
}

// Create a vector of const PasswordForm from a vector of
// unique_ptr<PasswordForm> by applying get() item-wise.
std::vector<const PasswordForm*> MakeWeakCopies(
    const std::vector<std::unique_ptr<PasswordForm>>& owning) {
  std::vector<const PasswordForm*> result(owning.size());
  std::transform(
      owning.begin(), owning.end(), result.begin(),
      [](const std::unique_ptr<PasswordForm>& ptr) { return ptr.get(); });
  return result;
}

// Create a vector of unique_ptr<PasswordForm> from another such vector by
// copying the pointed-to forms.
std::vector<std::unique_ptr<PasswordForm>> MakeCopies(
    const std::vector<std::unique_ptr<PasswordForm>>& source) {
  std::vector<std::unique_ptr<PasswordForm>> result(source.size());
  std::transform(source.begin(), source.end(), result.begin(),
                 [](const std::unique_ptr<PasswordForm>& ptr) {
                   return std::make_unique<PasswordForm>(*ptr);
                 });
  return result;
}

}  // namespace

FormFetcherImpl::FormFetcherImpl(PasswordStore::FormDigest form_digest,
                                 const PasswordManagerClient* client,
                                 bool should_migrate_http_passwords,
                                 bool should_query_suppressed_forms)
    : form_digest_(std::move(form_digest)),
      client_(client),
      should_migrate_http_passwords_(should_migrate_http_passwords),
      should_query_suppressed_forms_(should_query_suppressed_forms) {}

FormFetcherImpl::~FormFetcherImpl() = default;

void FormFetcherImpl::AddConsumer(FormFetcher::Consumer* consumer) {
  DCHECK(consumer);
  consumers_.insert(consumer);
  if (state_ == State::NOT_WAITING)
    consumer->ProcessMatches(weak_non_federated_, filtered_count_);
}

void FormFetcherImpl::RemoveConsumer(FormFetcher::Consumer* consumer) {
  size_t removed_consumers = consumers_.erase(consumer);
  DCHECK_EQ(1u, removed_consumers);
}

FormFetcherImpl::State FormFetcherImpl::GetState() const {
  return state_;
}

const std::vector<InteractionsStats>& FormFetcherImpl::GetInteractionsStats()
    const {
  return interactions_stats_;
}

const std::vector<const PasswordForm*>& FormFetcherImpl::GetFederatedMatches()
    const {
  return weak_federated_;
}

const std::vector<const PasswordForm*>&
FormFetcherImpl::GetSuppressedHTTPSForms() const {
  return weak_suppressed_same_origin_https_forms_;
}

const std::vector<const PasswordForm*>&
FormFetcherImpl::GetSuppressedPSLMatchingForms() const {
  return weak_suppressed_psl_matching_forms_;
}

const std::vector<const PasswordForm*>&
FormFetcherImpl::GetSuppressedSameOrganizationNameForms() const {
  return weak_suppressed_same_organization_name_forms_;
}

bool FormFetcherImpl::DidCompleteQueryingSuppressedForms() const {
  return did_complete_querying_suppressed_forms_;
}

void FormFetcherImpl::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);

  if (need_to_refetch_) {
    // The received results are no longer up to date, need to re-request.
    state_ = State::NOT_WAITING;
    Fetch();
    need_to_refetch_ = false;
    return;
  }

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_GET_STORE_RESULTS_METHOD);
    logger->LogNumber(Logger::STRING_NUMBER_RESULTS, results.size());
  }

  // Kick off the discovery of suppressed credentials, regardless of whether
  // there are some precisely matching |results|. These results are used only
  // for recording metrics at PasswordFormManager desctruction time, this is why
  // they are requested this late.
  if (should_query_suppressed_forms_ &&
      form_digest_.scheme == PasswordForm::SCHEME_HTML &&
      GURL(form_digest_.signon_realm).SchemeIsHTTPOrHTTPS()) {
    suppressed_form_fetcher_ = std::make_unique<SuppressedFormFetcher>(
        form_digest_.signon_realm, client_, this);
  }

  if (should_migrate_http_passwords_ && results.empty() &&
      form_digest_.origin.SchemeIs(url::kHttpsScheme)) {
    http_migrator_ = std::make_unique<HttpPasswordStoreMigrator>(
        form_digest_.origin, client_, this);
    return;
  }

  ProcessPasswordStoreResults(std::move(results));
}

void FormFetcherImpl::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {
  // On Windows the password request may be resolved after the statistics due to
  // importing from IE.
  interactions_stats_ = std::move(stats);
}

void FormFetcherImpl::ProcessMigratedForms(
    std::vector<std::unique_ptr<autofill::PasswordForm>> forms) {
  ProcessPasswordStoreResults(std::move(forms));
}

void FormFetcherImpl::ProcessSuppressedForms(
    std::vector<std::unique_ptr<autofill::PasswordForm>> forms) {
  did_complete_querying_suppressed_forms_ = true;
  SplitSuppressedFormsAndAssignTo(form_digest_, std::move(forms),
                                  &suppressed_same_origin_https_forms_,
                                  &suppressed_psl_matching_forms_,
                                  &suppressed_same_organization_name_forms_);
  weak_suppressed_same_origin_https_forms_ =
      MakeWeakCopies(suppressed_same_origin_https_forms_);
  weak_suppressed_psl_matching_forms_ =
      MakeWeakCopies(suppressed_psl_matching_forms_);
  weak_suppressed_same_organization_name_forms_ =
      MakeWeakCopies(suppressed_same_organization_name_forms_);
}

void FormFetcherImpl::Fetch() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
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

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    if (logger)
      logger->LogMessage(Logger::STRING_NO_STORE);
    NOTREACHED();
    return;
  }
  state_ = State::WAITING;
  password_store->GetLogins(form_digest_, this);

// The statistics isn't needed on mobile, only on desktop. Let's save some
// processor cycles.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // The statistics is needed for the "Save password?" bubble.
  password_store->GetSiteStats(form_digest_.origin.GetOrigin(), this);
#endif
}

std::unique_ptr<FormFetcher> FormFetcherImpl::Clone() {
  // Create the copy without the "HTTPS migration" activated. If it was needed,
  // then it was done by |this| already.
  auto result = std::make_unique<FormFetcherImpl>(
      form_digest_, client_, false, should_query_suppressed_forms_);

  if (state_ != State::NOT_WAITING) {
    // There are no store results to copy, trigger a Fetch on the clone instead.
    result->Fetch();
    return std::move(result);
  }

  result->non_federated_ = MakeCopies(this->non_federated_);
  result->federated_ = MakeCopies(this->federated_);
  result->interactions_stats_ = this->interactions_stats_;
  result->suppressed_same_origin_https_forms_ =
      MakeCopies(this->suppressed_same_origin_https_forms_);
  result->suppressed_psl_matching_forms_ =
      MakeCopies(this->suppressed_psl_matching_forms_);
  result->suppressed_same_organization_name_forms_ =
      MakeCopies(this->suppressed_same_organization_name_forms_);

  result->weak_non_federated_ = MakeWeakCopies(result->non_federated_);
  result->weak_federated_ = MakeWeakCopies(result->federated_);
  result->weak_suppressed_same_origin_https_forms_ =
      MakeWeakCopies(result->suppressed_same_origin_https_forms_);
  result->weak_suppressed_psl_matching_forms_ =
      MakeWeakCopies(result->suppressed_psl_matching_forms_);
  result->weak_suppressed_same_organization_name_forms_ =
      MakeWeakCopies(result->suppressed_same_organization_name_forms_);

  result->filtered_count_ = this->filtered_count_;
  result->state_ = this->state_;
  result->need_to_refetch_ = this->need_to_refetch_;

  return std::move(result);
}

void FormFetcherImpl::ProcessPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);
  state_ = State::NOT_WAITING;
  federated_ = SplitFederatedMatches(&results);
  non_federated_ = std::move(results);

  const size_t original_count = non_federated_.size();

  non_federated_ =
      client_->GetStoreResultFilter()->FilterResults(std::move(non_federated_));

  filtered_count_ = original_count - non_federated_.size();

  weak_non_federated_ = MakeWeakCopies(non_federated_);
  weak_federated_ = MakeWeakCopies(federated_);

  for (FormFetcher::Consumer* consumer : consumers_)
    consumer->ProcessMatches(weak_non_federated_, filtered_count_);
}

}  // namespace password_manager
