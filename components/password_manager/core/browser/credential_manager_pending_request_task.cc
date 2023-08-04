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
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/credential_manager_utils.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
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

using password_manager_util::GetLoginMatchType;
using password_manager_util::GetMatchType;

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
void FilterDuplicatesInFederatedCredentials(
    std::vector<std::unique_ptr<PasswordForm>>& forms) {
  auto federated_forms_with_unique_username =
      MakeFlatSet(/*key_getter=*/[](const auto& form) {
        return std::make_pair(form->username_value, form->federation_origin);
      });

  for (auto& form : forms) {
    CHECK(!form->federation_origin.opaque());
    // |forms| contains credentials from both the profile and account stores.
    // Therefore, it could potentially contains duplicate federated
    // credentials. In case of duplicates, favor the account store version.
    InsertOrReplaceIf(federated_forms_with_unique_username, std::move(form),
                      [](const auto& old_form, const auto& new_form) {
                        return new_form->IsUsingAccountStore();
                      });
  }

  forms = std::move(federated_forms_with_unique_username).extract();
}

void FilterIrrelevantForms(std::vector<std::unique_ptr<PasswordForm>>& forms,
                           bool include_passwords,
                           const std::set<std::string>& federations) {
  // Get rid of the irrelevant credentials.
  base::EraseIf(forms, [include_passwords, &federations](
                           const std::unique_ptr<PasswordForm>& form) {
    // Remove empty usernames from the list.
    if (form->username_value.empty()) {
      return true;
    }

    if (form->federation_origin.opaque()) {
      // Remove passwords if they shouldn't be included.
      return !include_passwords;
    }

    // Ensure that the form we're looking at matches federation filters
    // provided.
    return !federations.contains(form->federation_origin.Serialize());
  });
}

bool IsFormValidForAutoSignIn(const PasswordForm* form) {
  GetLoginMatchType match_type = GetMatchType(*form);
  // Only exactly matching form, or affiliated android app can be used for auto
  // sign in.
  if (match_type == GetLoginMatchType::kExact ||
      (match_type == GetLoginMatchType::kAffiliated &&
       IsValidAndroidFacetURI(form->signon_realm))) {
    return true;
  }
  return false;
}

}  // namespace

CredentialManagerPendingRequestTask::CredentialManagerPendingRequestTask(
    CredentialManagerPendingRequestTaskDelegate* delegate,
    SendCredentialCallback callback,
    CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& request_federations,
    PasswordFormDigest form_digest)
    : delegate_(delegate),
      send_callback_(std::move(callback)),
      mediation_(mediation),
      origin_(delegate_->GetOrigin()),
      include_passwords_(include_passwords),
      form_fetcher_(std::make_unique<FormFetcherImpl>(
          std::move(form_digest),
          delegate_->client(),
          /*should_migrate_http_passwords=*/true)) {
  CHECK(!net::IsCertStatusError(delegate_->client()->GetMainFrameCertStatus()));

  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);

  for (const GURL& federation : request_federations)
    federations_.insert(
        url::Origin::Create(federation.DeprecatedGetOriginAsURL()).Serialize());
}

CredentialManagerPendingRequestTask::~CredentialManagerPendingRequestTask() {
  form_fetcher_->RemoveConsumer(this);
}

void CredentialManagerPendingRequestTask::OnFetchCompleted() {
  std::vector<std::unique_ptr<PasswordForm>> all_matches;
  base::ranges::transform(form_fetcher_->GetFederatedMatches(),
                          std::back_inserter(all_matches),
                          [](const PasswordForm* form) {
                            return std::make_unique<PasswordForm>(*form);
                          });
  // GetFederatedMatches() comes with duplicates, filter them immediately.
  FilterDuplicatesInFederatedCredentials(all_matches);
  base::ranges::transform(form_fetcher_->GetBestMatches(),
                          std::back_inserter(all_matches),
                          [](const PasswordForm* form) {
                            return std::make_unique<PasswordForm>(*form);
                          });
  FilterIrrelevantForms(all_matches, include_passwords_, federations_);
  ProcessForms(std::move(all_matches));
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

  // We only perform zero-click sign-in when it is not forbidden via the
  // mediation requirement and the result is completely unambiguous.
  // If there is one and only one entry originated for this website, and
  // zero-click is enabled for that entry, return it.
  //
  // Moreover, we only return such a credential if the user has opted-in via the
  // first-run experience.
  const bool can_use_autosignin =
      mediation_ != CredentialMediationRequirement::kRequired &&
      results.size() == 1u && delegate_->IsZeroClickAllowed() &&
      IsFormValidForAutoSignIn(results[0].get());
  if (can_use_autosignin && !results[0]->skip_zero_click &&
      !password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          delegate_->client()->GetPrefs())) {
    auto info = PasswordFormToCredentialInfo(*results[0]);
    delegate_->client()->NotifyUserAutoSignin(std::move(results), origin_);
    base::RecordAction(base::UserMetricsAction("CredentialManager_Autosignin"));
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kAutoSignIn, mediation_);
    delegate_->SendCredential(std::move(send_callback_), info);
    return;
  }

  if (mediation_ == CredentialMediationRequirement::kSilent) {
    metrics_util::CredentialManagerGetResult get_result;
    if (results.empty()) {
      get_result = metrics_util::CredentialManagerGetResult::kNoneEmptyStore;
    } else if (results.size() > 1) {
      get_result =
          metrics_util::CredentialManagerGetResult::kNoneManyCredentials;
    } else if (results[0]->skip_zero_click) {
      get_result = metrics_util::CredentialManagerGetResult::kNoneSignedOut;
    } else {
      get_result = metrics_util::CredentialManagerGetResult::kNoneFirstRun;
    }

    if (!results.empty()) {
      std::vector<const PasswordForm*> non_federated_matches;
      std::vector<const PasswordForm*> federated_matches;
      for (const auto& result : results) {
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
      delegate_->client()->NotifyUserCouldBeAutoSignedIn(std::move(results[0]));
    }
    LogCredentialManagerGetResult(get_result, mediation_);
    delegate_->SendCredential(std::move(send_callback_), CredentialInfo());
    return;
  }

  if (results.empty()) {
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNoneEmptyStore, mediation_);
    delegate_->SendCredential(std::move(send_callback_), CredentialInfo());
    return;
  }

  auto split_send_callback = base::SplitOnceCallback(std::move(send_callback_));
  if (!delegate_->client()->PromptUserToChooseCredentials(
          std::move(results), origin_,
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
