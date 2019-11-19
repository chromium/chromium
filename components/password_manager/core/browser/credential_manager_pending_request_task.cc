// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

// Returns true iff |form1| is better suitable for showing in the account
// chooser than |form2|. Inspired by PasswordFormManager::ScoreResult.
bool IsBetterMatch(const autofill::PasswordForm& form1,
                   const autofill::PasswordForm& form2) {
  if (!form1.is_public_suffix_match && form2.is_public_suffix_match)
    return true;
  if (form1.preferred && !form2.preferred)
    return true;
  return form1.date_created > form2.date_created;
}

// Remove duplicates in |forms| before displaying them in the account chooser.
void FilterDuplicates(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> federated_forms;
  // The key is [username, signon_realm]. signon_realm is used only for PSL
  // matches because those entries have it in the UI.
  std::map<std::pair<base::string16, std::string>,
           std::unique_ptr<autofill::PasswordForm>>
      credentials;
  for (auto& form : *forms) {
    if (!form->federation_origin.opaque()) {
      federated_forms.push_back(std::move(form));
    } else {
      auto key = std::make_pair(
          form->username_value,
          form->is_public_suffix_match ? form->signon_realm : std::string());
      auto it = credentials.find(key);
      if (it == credentials.end() || IsBetterMatch(*form, *it->second))
        credentials[key] = std::move(form);
    }
  }
  forms->clear();
  for (auto& form_pair : credentials)
    forms->push_back(std::move(form_pair.second));
  std::move(federated_forms.begin(), federated_forms.end(),
            std::back_inserter(*forms));
}

// Sift |forms| for the account chooser so it doesn't have empty usernames or
// duplicates.
void FilterDuplicatesAndEmptyUsername(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) {
  // Remove empty usernames from the list.
  base::EraseIf(*forms,
                [](const std::unique_ptr<autofill::PasswordForm>& form) {
                  return form->username_value.empty();
                });

  FilterDuplicates(forms);
}

}  // namespace

CredentialManagerPendingRequestTask::CredentialManagerPendingRequestTask(
    CredentialManagerPendingRequestTaskDelegate* delegate,
    const SendCredentialCallback& callback,
    CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& request_federations)
    : delegate_(delegate),
      send_callback_(callback),
      mediation_(mediation),
      origin_(delegate_->GetOrigin()),
      include_passwords_(include_passwords) {
  CHECK(!net::IsCertStatusError(delegate_->client()->GetMainFrameCertStatus()));
  for (const GURL& federation : request_federations)
    federations_.insert(
        url::Origin::Create(federation.GetOrigin()).Serialize());
}

CredentialManagerPendingRequestTask::~CredentialManagerPendingRequestTask() =
    default;

void CredentialManagerPendingRequestTask::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  // localhost is a secure origin but not https.
  if (results.empty() && origin_.SchemeIs(url::kHttpsScheme)) {
    // Try to migrate the HTTP passwords and process them later.
    http_migrator_ = std::make_unique<HttpPasswordStoreMigrator>(
        origin_, delegate_->client(), this);
    return;
  }
  ProcessForms(std::move(results));
}

void CredentialManagerPendingRequestTask::ProcessMigratedForms(
    std::vector<std::unique_ptr<autofill::PasswordForm>> forms) {
  ProcessForms(std::move(forms));
}

void CredentialManagerPendingRequestTask::ProcessForms(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  using metrics_util::LogCredentialManagerGetResult;
  if (delegate_->GetOrigin() != origin_) {
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNone, mediation_);
    delegate_->SendCredential(send_callback_, CredentialInfo());
    return;
  }
  // Get rid of the blacklisted credentials.
  base::EraseIf(results,
                [](const std::unique_ptr<autofill::PasswordForm>& form) {
                  return form->blacklisted_by_user;
                });

  std::vector<std::unique_ptr<autofill::PasswordForm>> local_results;
  std::vector<std::unique_ptr<autofill::PasswordForm>> psl_results;
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
    if (form->is_affiliation_based_match ||
        form->origin.GetOrigin() == origin_.GetOrigin()) {
      local_results.push_back(std::move(form));
    } else if (form->is_public_suffix_match) {
      psl_results.push_back(std::move(form));
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
    CredentialInfo info(*local_results[0],
                        local_results[0]->federation_origin.opaque()
                            ? CredentialType::CREDENTIAL_TYPE_PASSWORD
                            : CredentialType::CREDENTIAL_TYPE_FEDERATED);
    delegate_->client()->NotifyUserAutoSignin(std::move(local_results),
                                              origin_);
    base::RecordAction(base::UserMetricsAction("CredentialManager_Autosignin"));
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kAutoSignIn, mediation_);
    delegate_->SendCredential(send_callback_, info);
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
      std::vector<const autofill::PasswordForm*> non_federated_matches;
      std::vector<const autofill::PasswordForm*> federated_matches;
      for (const auto& result : local_results) {
        if (result->IsFederatedCredential()) {
          federated_matches.emplace_back(result.get());
        } else {
          non_federated_matches.emplace_back(result.get());
        }
      }
      delegate_->client()->PasswordWasAutofilled(non_federated_matches, origin_,
                                                 &federated_matches);
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
    delegate_->SendCredential(send_callback_, CredentialInfo());
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
    delegate_->SendCredential(send_callback_, CredentialInfo());
    return;
  }

  if (!delegate_->client()->PromptUserToChooseCredentials(
          std::move(local_results), origin_,
          base::Bind(
              &CredentialManagerPendingRequestTaskDelegate::SendPasswordForm,
              base::Unretained(delegate_), send_callback_, mediation_))) {
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNone, mediation_);
    delegate_->SendCredential(send_callback_, CredentialInfo());
  }
}

}  // namespace password_manager
