// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/credential_manager_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

// Inserts `form` into `set` if no equally comparing element exists yet, or
// replaces an existing `old_form` if `pred(old_form, form)` evaluates to true.
// Returns whether `set` contains `form` following this operation.
template <typename Comp, typename Predicate>
bool InsertOrReplaceIf(base::flat_set<std::unique_ptr<PasswordForm>, Comp>& set,
                       std::unique_ptr<PasswordForm> form,
                       Predicate pred) {
  auto lower = set.lower_bound(form);
  if (lower == set.end() || set.key_comp()(form, *lower)) {
    set.insert(lower, std::move(form));
    return true;
  }

  if (pred(*lower, form)) {
    *lower = std::move(form);
    return true;
  }

  return false;
}

// Creates a base::flat_set of std::unique_ptr<PasswordForm> that uses
// |key_getter| to compute the key used when comparing forms.
template <typename KeyGetter>
auto MakeFlatSet(KeyGetter key_getter) {
  auto cmp = [key_getter](const auto& lhs, const auto& rhs) {
    return key_getter(lhs) < key_getter(rhs);
  };

  return base::flat_set<std::unique_ptr<PasswordForm>, decltype(cmp)>(cmp);
}

// Remove duplicates in |forms| before displaying them in the account chooser.
void FilterDuplicates(std::vector<std::unique_ptr<PasswordForm>>* forms) {
  auto federated_forms_with_unique_username =
      MakeFlatSet(/*key_getter=*/[](const auto& form) {
        return std::make_pair(form->username_value, form->federation_origin);
      });

  std::vector<const PasswordForm*> all_non_federated_forms;
  for (auto& form : *forms) {
    if (!form->federation_origin.opaque()) {
      // |forms| contains credentials from both the profile and account stores.
      // Therefore, it could potentially contains duplicate federated
      // credentials. In case of duplicates, favor the account store version.
      InsertOrReplaceIf(federated_forms_with_unique_username, std::move(form),
                        [](const auto& old_form, const auto& new_form) {
                          return new_form->IsUsingAccountStore();
                        });
    } else {
      all_non_federated_forms.push_back(form.get());
    }
  }

  std::vector<const PasswordForm*> other_matches;
  std::vector<const PasswordForm*> best_matches;
  password_manager_util::FindBestMatches(all_non_federated_forms,
                                         PasswordForm::Scheme::kHtml,
                                         &other_matches, &best_matches);

  std::vector<std::unique_ptr<PasswordForm>> result;
  for (const PasswordForm* best_match : best_matches) {
    auto it = base::ranges::find(*forms, best_match,
                                 &std::unique_ptr<PasswordForm>::get);
    DCHECK(it != forms->end());
    result.push_back(std::move(*it));
  }

  *forms = std::move(result);

  std::vector<std::unique_ptr<PasswordForm>> federated_forms =
      std::move(federated_forms_with_unique_username).extract();
  std::move(federated_forms.begin(), federated_forms.end(),
            std::back_inserter(*forms));
}

// Sift |forms| for the account chooser so it doesn't have empty usernames or
// duplicates.
void FilterDuplicatesAndEmptyUsername(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  // Remove empty usernames from the list.
  base::EraseIf(*forms, [](const std::unique_ptr<PasswordForm>& form) {
    return form->username_value.empty();
  });

  FilterDuplicates(forms);
}

}  // namespace

CredentialManagerPendingRequestTask::CredentialManagerPendingRequestTask(
    CredentialManagerPendingRequestTaskDelegate* delegate,
    SendCredentialCallback callback,
    CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& request_federations,
    StoresToQuery stores_to_query)
    : delegate_(delegate),
      send_callback_(std::move(callback)),
      mediation_(mediation),
      origin_(delegate_->GetOrigin()),
      include_passwords_(include_passwords) {
  CHECK(!net::IsCertStatusError(delegate_->client()->GetMainFrameCertStatus()));
  switch (stores_to_query) {
    case StoresToQuery::kProfileStore:
      expected_stores_to_respond_ = 1;
      break;
    case StoresToQuery::kProfileAndAccountStores:
      expected_stores_to_respond_ = 2;
      break;
  }

  for (const GURL& federation : request_federations)
    federations_.insert(
        url::Origin::Create(federation.DeprecatedGetOriginAsURL()).Serialize());
}

CredentialManagerPendingRequestTask::~CredentialManagerPendingRequestTask() =
    default;

void CredentialManagerPendingRequestTask::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // This class overrides OnGetPasswordStoreResultsFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void CredentialManagerPendingRequestTask::OnGetPasswordStoreResultsFrom(
    PasswordStoreInterface* store,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // localhost is a secure origin but not https.
  if (results.empty() && origin_.scheme() == url::kHttpsScheme) {
    // Try to migrate the HTTP passwords and process them later.
    http_migrators_[store] = std::make_unique<HttpPasswordStoreMigrator>(
        origin_, store, delegate_->client()->GetNetworkContext(), this);
    return;
  }
  AggregatePasswordStoreResults(std::move(results));
}

base::WeakPtr<PasswordStoreConsumer>
CredentialManagerPendingRequestTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CredentialManagerPendingRequestTask::ProcessMigratedForms(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  AggregatePasswordStoreResults(std::move(forms));
}

void CredentialManagerPendingRequestTask::AggregatePasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Store the results.
  for (auto& form : results)
    partial_results_.push_back(std::move(form));

  // If we're still awaiting more results, nothing else to do.
  if (--expected_stores_to_respond_ > 0)
    return;
  ProcessForms(std::move(partial_results_));
}

void CredentialManagerPendingRequestTask::ProcessForms(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  using metrics_util::LogCredentialManagerGetResult;
  if (delegate_->GetOrigin() != origin_) {
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNone, mediation_);
    delegate_->SendCredential(std::move(send_callback_), CredentialInfo());
    return;
  }
  // Get rid of the blocked credentials.
  base::EraseIf(results, [](const std::unique_ptr<PasswordForm>& form) {
    return form->blocked_by_user;
  });

  std::vector<std::unique_ptr<PasswordForm>> local_results;
  std::vector<std::unique_ptr<PasswordForm>> psl_results;
  for (auto& form : results) {
    // Ensure that the form we're looking at matches the password and
    // federation filters provided.
    if (!((form->federation_origin.opaque() && include_passwords_) ||
          (!form->federation_origin.opaque() &&
           federations_.count(form->federation_origin.Serialize())))) {
      continue;
    }

    // PasswordFrom and GURL have different definition of origin.
    // PasswordForm definition: scheme, host, port and path.
    // GURL definition: scheme, host, and port.
    // So we can't compare them directly.
    if (password_manager_util::GetMatchType(*form) ==
        password_manager_util::GetLoginMatchType::kPSL) {
      psl_results.push_back(std::move(form));
    } else {
      local_results.push_back(std::move(form));
    }
  }

  FilterDuplicatesAndEmptyUsername(&local_results);

  // We only perform zero-click sign-in when it is not forbidden via the
  // mediation requirement and the result is completely unambigious.
  // If there is one and only one entry, and zero-click is
  // enabled for that entry, return it.
  //
  // Moreover, we only return such a credential if the user has opted-in via the
  // first-run experience.
  const bool can_use_autosignin =
      mediation_ != CredentialMediationRequirement::kRequired &&
      local_results.size() == 1u && delegate_->IsZeroClickAllowed();
  if (can_use_autosignin && !local_results[0]->skip_zero_click &&
      !password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          delegate_->client()->GetPrefs())) {
    auto info = PasswordFormToCredentialInfo(*local_results[0]);
    delegate_->client()->NotifyUserAutoSignin(std::move(local_results),
                                              origin_);
    base::RecordAction(base::UserMetricsAction("CredentialManager_Autosignin"));
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kAutoSignIn, mediation_);
    delegate_->SendCredential(std::move(send_callback_), info);
    return;
  }

  if (mediation_ == CredentialMediationRequirement::kSilent) {
    metrics_util::CredentialManagerGetResult get_result;
    if (local_results.empty())
      get_result = metrics_util::CredentialManagerGetResult::kNoneEmptyStore;
    else if (!can_use_autosignin)
      get_result =
          metrics_util::CredentialManagerGetResult::kNoneManyCredentials;
    else if (local_results[0]->skip_zero_click)
      get_result = metrics_util::CredentialManagerGetResult::kNoneSignedOut;
    else
      get_result = metrics_util::CredentialManagerGetResult::kNoneFirstRun;

    if (!local_results.empty()) {
      std::vector<const PasswordForm*> non_federated_matches;
      std::vector<const PasswordForm*> federated_matches;
      for (const auto& result : local_results) {
        if (result->IsFederatedCredential()) {
          federated_matches.emplace_back(result.get());
        } else {
          non_federated_matches.emplace_back(result.get());
        }
      }
      delegate_->client()->PasswordWasAutofilled(
          non_federated_matches, origin_, &federated_matches,
          /*was_autofilled_on_pageload=*/false);
    }
    if (can_use_autosignin) {
      // The user had credentials, but either chose not to share them with the
      // site, or was prevented from doing so by lack of zero-click (or the
      // first-run experience). So, notify the client that we could potentially
      // have used zero-click.
      delegate_->client()->NotifyUserCouldBeAutoSignedIn(
          std::move(local_results[0]));
    }
    LogCredentialManagerGetResult(get_result, mediation_);
    delegate_->SendCredential(std::move(send_callback_), CredentialInfo());
    return;
  }

  // Time to show the account chooser. If |local_results| is empty then it
  // should list the PSL matches.
  if (local_results.empty()) {
    local_results = std::move(psl_results);
    FilterDuplicatesAndEmptyUsername(&local_results);
  }

  if (local_results.empty()) {
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNoneEmptyStore, mediation_);
    delegate_->SendCredential(std::move(send_callback_), CredentialInfo());
    return;
  }

  auto split_send_callback = base::SplitOnceCallback(std::move(send_callback_));
  if (!delegate_->client()->PromptUserToChooseCredentials(
          std::move(local_results), origin_,
          base::BindOnce(
              &CredentialManagerPendingRequestTaskDelegate::SendPasswordForm,
              base::Unretained(delegate_), std::move(split_send_callback.first),
              mediation_))) {
    // Since PromptUserToChooseCredentials() does not invoke the callback when
    // returning false, `repeating_send_callback` has not been run in this
    // branch yet.
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNone, mediation_);
    delegate_->SendCredential(std::move(split_send_callback.second),
                              CredentialInfo());
  }
}

}  // namespace password_manager
