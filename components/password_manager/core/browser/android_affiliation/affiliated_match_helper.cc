// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

namespace {

bool IsFacetValidForAffiliation(const FacetURI& facet) {
  return facet.IsValidAndroidFacetURI() ||
         (facet.IsValidWebFacetURI() &&
          base::FeatureList::IsEnabled(
              password_manager::features::kFillingAcrossAffiliatedWebsites));
}

}  // namespace

// static
constexpr base::TimeDelta AffiliatedMatchHelper::kInitializationDelayOnStartup;

AffiliatedMatchHelper::AffiliatedMatchHelper(
    AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {}

AffiliatedMatchHelper::~AffiliatedMatchHelper() {
  if (password_store_)
    password_store_->RemoveObserver(this);
}

void AffiliatedMatchHelper::Initialize(PasswordStoreInterface* password_store) {
  DCHECK(password_store);
  DCHECK(affiliation_service_);
  password_store_ = password_store;
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

void AffiliatedMatchHelper::DoDeferredInitialization() {
  // Must start observing for changes at the same time as when the snapshot is
  // taken to avoid inconsistencies due to any changes taking place in-between.
  password_store_->AddObserver(this);
  password_store_->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
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
    const std::vector<PasswordForm>& retained_passwords) {
  std::vector<FacetURI> facets;
  for (const auto& form : retained_passwords) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
    if (IsFacetValidForAffiliation(facet_uri))
      facets.push_back(std::move(facet_uri));
  }
  affiliation_service_->KeepPrefetchForFacets(std::move(facets));
}

void AffiliatedMatchHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  std::vector<FacetURI> facets;
  for (const auto& form : results) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm);
    if (IsFacetValidForAffiliation(facet_uri))
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());

    facets.push_back(std::move(facet_uri));
  }
  affiliation_service_->TrimUnusedCache(std::move(facets));
}

}  // namespace password_manager
