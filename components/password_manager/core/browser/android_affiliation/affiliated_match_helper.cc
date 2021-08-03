// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"

#include <algorithm>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/password_manager/core/browser/android_affiliation/android_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

// static
constexpr base::TimeDelta AffiliatedMatchHelper::kInitializationDelayOnStartup;

AffiliatedMatchHelper::AffiliatedMatchHelper(
    PasswordStore* password_store,
    std::unique_ptr<AndroidAffiliationService> affiliation_service)
    : password_store_(password_store),
      affiliation_service_(std::move(affiliation_service)) {}

AffiliatedMatchHelper::~AffiliatedMatchHelper() {
  if (password_store_)
    password_store_->RemoveObserver(this);
}

void AffiliatedMatchHelper::Initialize() {
  DCHECK(password_store_);
  DCHECK(affiliation_service_);
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AffiliatedMatchHelper::DoDeferredInitialization,
                     weak_ptr_factory_.GetWeakPtr()),
      kInitializationDelayOnStartup);
}

void AffiliatedMatchHelper::GetAffiliatedAndroidAndWebRealms(
    const PasswordFormDigest& observed_form,
    AffiliatedRealmsCallback result_callback) {
  if (IsValidWebCredential(observed_form)) {
    FacetURI facet_uri(
        FacetURI::FromPotentiallyInvalidSpec(observed_form.signon_realm));
    affiliation_service_->GetAffiliationsAndBranding(
        facet_uri, AndroidAffiliationService::StrategyOnCacheMiss::FAIL,
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
    AndroidAffiliationService::StrategyOnCacheMiss strategy_on_cache_miss,
    PasswordFormsCallback result_callback) {
  std::vector<PasswordForm*> android_credentials;
  for (const auto& form : forms) {
    if (IsValidAndroidCredential(PasswordFormDigest(*form)))
      android_credentials.push_back(form.get());
  }
  base::OnceClosure on_get_all_realms(
      base::BindOnce(std::move(result_callback), std::move(forms)));
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      android_credentials.size(), std::move(on_get_all_realms));
  for (auto* form : android_credentials) {
    affiliation_service_->GetAffiliationsAndBranding(
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm),
        strategy_on_cache_miss,
        base::BindOnce(&AffiliatedMatchHelper::
                           CompleteInjectAffiliationAndBrandingInformation,
                       weak_ptr_factory_.GetWeakPtr(), base::Unretained(form),
                       barrier_closure));
  }
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
  auto affiliated_facet = std::find_if(
      results.begin(), results.end(), [](const Facet& affiliated_facet) {
        return affiliated_facet.uri.IsValidWebFacetURI();
      });
  if (affiliated_facet != results.end())
    form->affiliated_web_realm = affiliated_facet->uri.canonical_spec() + "/";

  std::move(barrier_closure).Run();
}

// static
bool AffiliatedMatchHelper::IsValidAndroidCredential(
    const PasswordFormDigest& form) {
  return form.scheme == PasswordForm::Scheme::kHtml &&
         IsValidAndroidFacetURI(form.signon_realm);
}

// static
bool AffiliatedMatchHelper::IsValidWebCredential(
    const PasswordFormDigest& form) {
  FacetURI facet_uri(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm));
  return form.scheme == PasswordForm::Scheme::kHtml &&
         facet_uri.IsValidWebFacetURI();
}

void AffiliatedMatchHelper::DoDeferredInitialization() {
  // Must start observing for changes at the same time as when the snapshot is
  // taken to avoid inconsistencies due to any changes taking place in-between.
  password_store_->AddObserver(this);
  password_store_->GetAllLogins(this);
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

void AffiliatedMatchHelper::OnLoginsChanged(
    PasswordStoreInterface* /*store*/,
    const PasswordStoreChangeList& changes) {
  std::vector<FacetURI> facet_uris_to_trim;
  for (const PasswordStoreChange& change : changes) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(change.form().signon_realm);

    if (!facet_uri.is_valid())
      continue;
    // Require a valid Android Facet if filling across affiliated websites is
    // disabled.
    if (!facet_uri.IsValidAndroidFacetURI() &&
        !base::FeatureList::IsEnabled(
            features::kFillingAcrossAffiliatedWebsites)) {
      continue;
    }

    if (change.type() == PasswordStoreChange::ADD) {
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
    } else if (change.type() == PasswordStoreChange::REMOVE) {
      // Stop keeping affiliation information fresh for deleted Android logins,
      // and make a note to potentially remove any unneeded cached data later.
      facet_uris_to_trim.push_back(facet_uri);
      affiliation_service_->CancelPrefetch(facet_uri, base::Time::Max());
    }
  }

  // When the primary key for a login is updated, |changes| will contain both a
  // REMOVE and ADD change for that login. Cached affiliation data should not be
  // deleted in this case. A simple solution is to call TrimCacheForFacetURI()
  // always after Prefetch() calls -- the trimming logic will detect that there
  // is an active prefetch and not delete the corresponding data.
  for (const FacetURI& facet_uri : facet_uris_to_trim)
    affiliation_service_->TrimCacheForFacetURI(facet_uri);
}

void AffiliatedMatchHelper::OnLoginsRetained(
    PasswordStoreInterface* /*store*/,
    const std::vector<PasswordForm>& retained_passwords) {}

void AffiliatedMatchHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  for (const auto& form : results) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm);
    if (facet_uri.IsValidAndroidFacetURI())
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());

    if (facet_uri.IsValidWebFacetURI() &&
        base::FeatureList::IsEnabled(
            password_manager::features::kFillingAcrossAffiliatedWebsites))
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
  }
}

}  // namespace password_manager
