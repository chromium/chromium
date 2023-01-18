// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

namespace {

bool IsValidAndroidCredential(PasswordForm* form) {
  return form->scheme == PasswordForm::Scheme::kHtml &&
         IsValidAndroidFacetURI(form->signon_realm);
}

}  // namespace

AffiliatedMatchHelper::AffiliatedMatchHelper(
    AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {
  DCHECK(affiliation_service_);
}

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

void AffiliatedMatchHelper::InjectAffiliationAndBrandingInformation(
    std::vector<std::unique_ptr<PasswordForm>> forms,
    PasswordFormsOrErrorCallback result_callback) {
  std::vector<PasswordForm*> android_credentials;
  for (const auto& form : forms) {
    if (IsValidAndroidCredential(form.get())) {
      android_credentials.push_back(form.get());
    }
  }

  // Create a closure that runs after affiliations are injected and
  // CompleteInjectAffiliationAndBrandingInformation is called for
  // all forms in |android_credentials|.
  base::OnceClosure on_get_all_realms(
      base::BindOnce(std::move(result_callback), std::move(forms)));
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      android_credentials.size(), std::move(on_get_all_realms));

  for (auto* form : android_credentials) {
    // |forms| are not destroyed until the |barrier_closure| executes,
    // making it safe to use base::Unretained(form) below.
    affiliation_service_->GetAffiliationsAndBranding(
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm),
        AffiliationService::StrategyOnCacheMiss::FAIL,
        base::BindOnce(&AffiliatedMatchHelper::
                           CompleteInjectAffiliationAndBrandingInformation,
                       weak_ptr_factory_.GetWeakPtr(), base::Unretained(form),
                       barrier_closure));
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

void AffiliatedMatchHelper::CompleteInjectAffiliationAndBrandingInformation(
    PasswordForm* form,
    base::OnceClosure barrier_closure,
    const AffiliatedFacets& results,
    bool success) {
  const FacetURI facet_uri(
      FacetURI::FromPotentiallyInvalidSpec(form->signon_realm));

  // Facet can also be web URI, in this case we do nothing.
  if (!success || !facet_uri.IsValidAndroidFacetURI()) {
    std::move(barrier_closure).Run();
    return;
  }

  // Inject branding information into the form (e.g. the Play Store name and
  // icon URL). We expect to always find a matching facet URI in the results.
  auto facet = base::ranges::find(results, facet_uri, &Facet::uri);

  DCHECK(facet != results.end());
  form->app_display_name = facet->branding_info.name;
  form->app_icon_url = facet->branding_info.icon_url;

  // Inject the affiliated web realm into the form, if available. In case
  // multiple web realms are available, this will always choose the first
  // available web realm for injection.
  auto affiliated_facet =
      base::ranges::find_if(results, [](const Facet& affiliated_facet) {
        return affiliated_facet.uri.IsValidWebFacetURI();
      });
  if (affiliated_facet != results.end()) {
    form->affiliated_web_realm = affiliated_facet->uri.canonical_spec() + "/";
  }

  std::move(barrier_closure).Run();
}

}  // namespace password_manager
