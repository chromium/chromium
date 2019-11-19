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
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"

namespace password_manager {

namespace {

// Returns whether or not |form| represents a credential for an Android
// application, and if so, returns the |facet_uri| of that application.
bool IsAndroidApplicationCredential(const autofill::PasswordForm& form,
                                    FacetURI* facet_uri) {
  DCHECK(facet_uri);
  if (form.scheme != autofill::PasswordForm::Scheme::kHtml)
    return false;

  *facet_uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  return facet_uri->IsValidAndroidFacetURI();
}

}  // namespace

// static
constexpr base::TimeDelta AffiliatedMatchHelper::kInitializationDelayOnStartup;

AffiliatedMatchHelper::AffiliatedMatchHelper(
    PasswordStore* password_store,
    std::unique_ptr<AffiliationService> affiliation_service)
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

void AffiliatedMatchHelper::GetAffiliatedAndroidRealms(
    const PasswordStore::FormDigest& observed_form,
    AffiliatedRealmsCallback result_callback) {
  if (IsValidWebCredential(observed_form)) {
    FacetURI facet_uri(
        FacetURI::FromPotentiallyInvalidSpec(observed_form.signon_realm));
    affiliation_service_->GetAffiliationsAndBranding(
        facet_uri, AffiliationService::StrategyOnCacheMiss::FAIL,
        base::BindOnce(
            &AffiliatedMatchHelper::CompleteGetAffiliatedAndroidRealms,
            weak_ptr_factory_.GetWeakPtr(), facet_uri,
            std::move(result_callback)));
  } else {
    std::move(result_callback).Run(std::vector<std::string>());
  }
}

void AffiliatedMatchHelper::GetAffiliatedWebRealms(
    const PasswordStore::FormDigest& android_form,
    AffiliatedRealmsCallback result_callback) {
  if (IsValidAndroidCredential(android_form)) {
    affiliation_service_->GetAffiliationsAndBranding(
        FacetURI::FromPotentiallyInvalidSpec(android_form.signon_realm),
        AffiliationService::StrategyOnCacheMiss::FETCH_OVER_NETWORK,
        base::BindOnce(&AffiliatedMatchHelper::CompleteGetAffiliatedWebRealms,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(result_callback)));
  } else {
    std::move(result_callback).Run(std::vector<std::string>());
  }
}

void AffiliatedMatchHelper::InjectAffiliationAndBrandingInformation(
    std::vector<std::unique_ptr<autofill::PasswordForm>> forms,
    PasswordFormsCallback result_callback) {
  std::vector<autofill::PasswordForm*> android_credentials;
  for (const auto& form : forms) {
    if (IsValidAndroidCredential(PasswordStore::FormDigest(*form)))
      android_credentials.push_back(form.get());
  }
  base::OnceClosure on_get_all_realms(
      base::BindOnce(std::move(result_callback), std::move(forms)));
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      android_credentials.size(), std::move(on_get_all_realms));
  for (auto* form : android_credentials) {
    affiliation_service_->GetAffiliationsAndBranding(
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm),
        AffiliationService::StrategyOnCacheMiss::FAIL,
        base::BindOnce(&AffiliatedMatchHelper::
                           CompleteInjectAffiliationAndBrandingInformation,
                       weak_ptr_factory_.GetWeakPtr(), base::Unretained(form),
                       barrier_closure));
  }
}

void AffiliatedMatchHelper::CompleteInjectAffiliationAndBrandingInformation(
    autofill::PasswordForm* form,
    base::OnceClosure barrier_closure,
    const AffiliatedFacets& results,
    bool success) {
  if (!success) {
    std::move(barrier_closure).Run();
    return;
  }

  const FacetURI facet_uri(
      FacetURI::FromPotentiallyInvalidSpec(form->signon_realm));
  DCHECK(facet_uri.IsValidAndroidFacetURI());

  // Inject branding information into the form (e.g. the Play Store name and
  // icon URL). We expect to always find a matching facet URI in the results.
  auto facet = std::find_if(results.begin(), results.end(),
                            [&facet_uri](const Facet& affiliated_facet) {
                              return affiliated_facet.uri == facet_uri;
                            });
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
    const PasswordStore::FormDigest& form) {
  return form.scheme == autofill::PasswordForm::Scheme::kHtml &&
         IsValidAndroidFacetURI(form.signon_realm);
}

// static
bool AffiliatedMatchHelper::IsValidWebCredential(
    const PasswordStore::FormDigest& form) {
  FacetURI facet_uri(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm));
  return form.scheme == autofill::PasswordForm::Scheme::kHtml &&
         facet_uri.IsValidWebFacetURI();
}

void AffiliatedMatchHelper::DoDeferredInitialization() {
  // Must start observing for changes at the same time as when the snapshot is
  // taken to avoid inconsistencies due to any changes taking place in-between.
  password_store_->AddObserver(this);
  password_store_->GetAllLogins(this);
}

void AffiliatedMatchHelper::CompleteGetAffiliatedAndroidRealms(
    const FacetURI& original_facet_uri,
    AffiliatedRealmsCallback result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (success) {
    for (const Facet& affiliated_facet : results) {
      if (affiliated_facet.uri != original_facet_uri &&
          affiliated_facet.uri.IsValidAndroidFacetURI())
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.uri.canonical_spec() +
                                    "/");
    }
  }
  std::move(result_callback).Run(affiliated_realms);
}

void AffiliatedMatchHelper::CompleteGetAffiliatedWebRealms(
    AffiliatedRealmsCallback result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (success) {
    for (const Facet& affiliated_facet : results) {
      if (affiliated_facet.uri.IsValidWebFacetURI())
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.uri.canonical_spec() +
                                    "/");
    }
  }
  std::move(result_callback).Run(affiliated_realms);
}

void AffiliatedMatchHelper::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  std::vector<FacetURI> facet_uris_to_trim;
  for (const PasswordStoreChange& change : changes) {
    FacetURI facet_uri;
    if (!IsAndroidApplicationCredential(change.form(), &facet_uri))
      continue;

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

void AffiliatedMatchHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  for (const auto& form : results) {
    FacetURI facet_uri;
    if (IsAndroidApplicationCredential(*form, &facet_uri))
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
  }
}

}  // namespace password_manager
