// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

AffiliatedMatchHelper::AffiliatedMatchHelper(
    AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {}

AffiliatedMatchHelper::~AffiliatedMatchHelper() = default;

void AffiliatedMatchHelper::GetAffiliatedAndroidAndWebRealms(
    const PasswordFormDigest& observed_form,
    AffiliatedRealmsCallback result_callback) {
  if (IsValidWebCredential(observed_form)) {
    FacetURI facet_uri(
        FacetURI::FromPotentiallyInvalidSpec(observed_form.signon_realm));
    affiliation_service_->GetAffiliationsAndBranding(
        facet_uri, AffiliationService::StrategyOnCacheMiss::FAIL,
        base::BindOnce(
            &AffiliatedMatchHelper::CompleteGetAffiliatedAndroidAndWebRealms,
            weak_ptr_factory_.GetWeakPtr(), facet_uri,
            std::move(result_callback)));
  } else {
    std::move(result_callback).Run(std::vector<std::string>());
  }
}

// static
bool AffiliatedMatchHelper::IsValidWebCredential(
    const PasswordFormDigest& form) {
  FacetURI facet_uri(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm));
  return form.scheme == PasswordForm::Scheme::kHtml &&
         facet_uri.IsValidWebFacetURI();
}

void AffiliatedMatchHelper::CompleteGetAffiliatedAndroidAndWebRealms(
    const FacetURI& original_facet_uri,
    AffiliatedRealmsCallback result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (!success) {
    std::move(result_callback).Run(affiliated_realms);
    return;
  }
  affiliated_realms.reserve(results.size());
  for (const Facet& affiliated_facet : results) {
    if (affiliated_facet.uri != original_facet_uri) {
      if (affiliated_facet.uri.IsValidAndroidFacetURI()) {
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.uri.canonical_spec() +
                                    "/");
      } else if (base::FeatureList::IsEnabled(
                     features::kFillingAcrossAffiliatedWebsites) &&
                 affiliated_facet.uri.IsValidWebFacetURI()) {
        DCHECK(!base::EndsWith(affiliated_facet.uri.canonical_spec(), "/"));
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.uri.canonical_spec() +
                                    "/");
      }
    }
  }
  std::move(result_callback).Run(affiliated_realms);
}

}  // namespace password_manager
