// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/trace_event.h"

#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "url/origin.h"

namespace password_manager {

namespace {

using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;
using LoginsResultOrError =
    absl::variant<LoginsResult, PasswordStoreBackendError>;

bool FormSupportsPSL(const PasswordFormDigest& digest) {
  return digest.scheme == PasswordForm::Scheme::kHtml &&
         !GetRegistryControlledDomain(GURL(digest.signon_realm)).empty();
}

// Do post-processing on forms and mark PSL matches as such.
LoginsResultOrError ProccessExactAndPSLForms(
    const PasswordFormDigest& digest,
    LoginsResultOrError logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
    return logins_or_error;
  }

  for (auto& form : absl::get<LoginsResult>(logins_or_error)) {
    switch (GetMatchResult(*form, digest)) {
      case MatchResult::NO_MATCH:
        NOTREACHED_NORETURN();
      case MatchResult::EXACT_MATCH:
      case MatchResult::FEDERATED_MATCH:
        break;
      case MatchResult::PSL_MATCH:
      case MatchResult::FEDERATED_PSL_MATCH:
        form->is_public_suffix_match = true;
        break;
    }
  }
  return logins_or_error;
}

class GetLoginsHelper : public base::RefCounted<GetLoginsHelper> {
 public:
  GetLoginsHelper(PasswordFormDigest requested_digest,
                  PasswordStoreBackend* backend)
      : requested_digest_(std::move(requested_digest)), backend_(backend) {}

  void Init(AffiliatedMatchHelper* affiliated_match_helper,
            LoginsOrErrorReply callback);

 private:
  friend class base::RefCounted<GetLoginsHelper>;
  ~GetLoginsHelper() = default;

  // From the affiliated realms returns all the forms to be additionally queried
  // in the password store. The list excludes the PSL matches because those will
  // be already returned by the main request.
  void HandleAffiliationsAndGroupsReceived(
      base::RepeatingCallback<void(LoginsResultOrError)>
          forms_received_callback,
      std::vector<std::string> affiliated_realms,
      std::vector<std::string> grouped_realms);

  // Method which is called after exact, PSL, affiliated and grouped matches
  // are received.
  LoginsResultOrError MergeResults(std::vector<LoginsResultOrError> results);

  const PasswordFormDigest requested_digest_;

  // All the affiliations for 'requested_digest_'.
  base::flat_set<std::string> affiliations_;

  // The group realms for 'requested_digest_'.
  base::flat_set<std::string> group_;

  raw_ptr<PasswordStoreBackend, FlakyDanglingUntriaged> backend_;
};

void GetLoginsHelper::Init(AffiliatedMatchHelper* affiliated_match_helper,
                           LoginsOrErrorReply callback) {
  // Number of time 'forms_received_' closure should be called before executing.
  // Once for perfect matches and once for affiliations.
  const int kCallsNumber = 2;

  auto forms_received_callback = base::BarrierCallback<LoginsResultOrError>(
      kCallsNumber, base::BindOnce(&GetLoginsHelper::MergeResults, this)
                        .Then(std::move(callback)));

  backend_->FillMatchingLoginsAsync(
      base::BindOnce(&ProccessExactAndPSLForms, requested_digest_)
          .Then(forms_received_callback),
      FormSupportsPSL(requested_digest_), {requested_digest_});

  affiliated_match_helper->GetAffiliatedAndGroupedRealms(
      requested_digest_,
      base::BindOnce(&GetLoginsHelper::HandleAffiliationsAndGroupsReceived,
                     this, std::move(forms_received_callback)));
}

void GetLoginsHelper::HandleAffiliationsAndGroupsReceived(
    base::RepeatingCallback<void(LoginsResultOrError)> forms_received_callback,
    std::vector<std::string> affiliated_realms,
    std::vector<std::string> grouped_realms) {
  affiliations_ = base::flat_set<std::string>(
      std::make_move_iterator(affiliated_realms.begin()),
      std::make_move_iterator(affiliated_realms.end()));
  group_ = base::flat_set<std::string>(
      std::make_move_iterator(grouped_realms.begin()),
      std::make_move_iterator(grouped_realms.end()));

  std::vector<PasswordFormDigest> digests_to_request;
  for (const auto& realm : affiliations_) {
    // The PSL forms are requested in the main request.
    if (!IsPublicSuffixDomainMatch(realm, requested_digest_.signon_realm)) {
      digests_to_request.emplace_back(requested_digest_.scheme, realm,
                                      GURL(realm));
    }
  }
  for (const auto& realm : group_) {
    // The PSL forms are requested in the main request, ignore affiliated
    // matches too.
    if (!IsPublicSuffixDomainMatch(realm, requested_digest_.signon_realm) &&
        !base::Contains(affiliations_, realm)) {
      digests_to_request.emplace_back(requested_digest_.scheme, realm,
                                      GURL(realm));
    }
  }
  backend_->FillMatchingLoginsAsync(std::move(forms_received_callback),
                                    /*include_psl=*/false, digests_to_request);
}

LoginsResultOrError GetLoginsHelper::MergeResults(
    std::vector<LoginsResultOrError> results) {
  LoginsResult final_result;
  for (auto& result : results) {
    if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
      return absl::get<PasswordStoreBackendError>(result);
    }
    LoginsResult forms = std::move(absl::get<LoginsResult>(result));
    for (auto& form : forms) {
      final_result.push_back(std::move(form));
    }
  }

  // PSL matches can also be affiliation/grouped matches.
  for (const auto& form : final_result) {
    switch (GetMatchResult(*form, requested_digest_)) {
      case MatchResult::EXACT_MATCH:
      case MatchResult::FEDERATED_MATCH:
        break;
      case MatchResult::NO_MATCH:
      case MatchResult::PSL_MATCH:
      case MatchResult::FEDERATED_PSL_MATCH: {
        std::string signon_realm = form->signon_realm;
        // For web federated credentials the signon_realm has a different
        // style. Extract the origin from URL instead for the lookup.
        if (form->IsFederatedCredential() &&
            !IsValidAndroidFacetURI(form->signon_realm)) {
          signon_realm = url::Origin::Create(form->url).GetURL().spec();
        }
        if (base::Contains(affiliations_, signon_realm)) {
          form->is_affiliation_based_match = true;
        }
        if (base::Contains(group_, signon_realm)) {
          form->is_grouped_match = true;
          // TODO(crbug.com/1432264): Delete after proper handling of
          // affiliated groups filling is implemented.
          form->is_affiliation_based_match = true;
        }
        break;
      }
    }
  }

  password_manager_util::TrimUsernameOnlyCredentials(&final_result);
  return final_result;
}

}  // namespace

void GetLoginsWithAffiliationsRequestHandler(
    PasswordFormDigest form,
    PasswordStoreBackend* backend,
    AffiliatedMatchHelper* affiliated_match_helper,
    LoginsOrErrorReply callback) {
  scoped_refptr<GetLoginsHelper> request_handler =
      base::MakeRefCounted<GetLoginsHelper>(std::move(form), backend);
  request_handler->Init(affiliated_match_helper, std::move(callback));
}

}  // namespace password_manager
