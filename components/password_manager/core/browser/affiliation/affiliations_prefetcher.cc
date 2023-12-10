// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/barrier_callback.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

namespace password_manager {

namespace {

constexpr base::TimeDelta kInitializationDelayOnStartup = base::Seconds(30);

// Filling across affiliated sites is implemented differently on Android.
bool IsFacetValidForAffiliation(const FacetURI& facet) {
#if BUILDFLAG(IS_ANDROID)
  return facet.IsValidAndroidFacetURI();
#else
  return facet.IsValidAndroidFacetURI() || facet.IsValidWebFacetURI();
#endif
}

std::optional<FacetURI> FacetURIFromPasskey(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  std::string as_url = base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, passkey.rp_id()});
  FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(as_url);
  if (!facet_uri.is_valid()) {
    return std::nullopt;
  }
  if (!IsFacetValidForAffiliation(facet_uri)) {
    return std::nullopt;
  }
  return facet_uri;
}

}  // namespace

AffiliationsPrefetcher::AffiliationsPrefetcher(
    AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {
  // I/O heavy initialization on start-up will be delayed by this long.
  // This should be high enough not to exacerbate start-up I/O contention too
  // much, but also low enough that the user be able log-in shortly after
  // browser start-up into web sites using Android credentials.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AffiliationsPrefetcher::InitializeWithPasswordStores,
                     weak_ptr_factory_.GetWeakPtr()),
      kInitializationDelayOnStartup);
}

AffiliationsPrefetcher::~AffiliationsPrefetcher() = default;

void AffiliationsPrefetcher::RegisterPasswordStore(
    PasswordStoreInterface* store) {
  DCHECK(store);

  pending_initializations_.push_back(store);
  // If initialization had already happened, request passwords from all stores
  // again to ensure affiliations cache gets properly updated, otherwise
  // do nothing.
  if (is_ready_) {
    InitializeWithPasswordStores();
  }
}

void AffiliationsPrefetcher::RegisterPasskeyModel(
    webauthn::PasskeyModel* passkey_model) {
  passkey_model_observation_.Observe(passkey_model);

  // If initialization had already happened, immediately prefetch affiliation
  // info for all passkeys.
  if (is_ready_) {
    for (const auto& passkey : passkey_model->GetAllPasskeys()) {
      std::optional<FacetURI> facet = FacetURIFromPasskey(passkey);
      if (facet) {
        affiliation_service_->Prefetch(std::move(*facet), base::Time::Max());
      }
    }
  }
}

void AffiliationsPrefetcher::Shutdown() {
  for (const auto& store : password_stores_) {
    store->RemoveObserver(this);
  }
  password_stores_.clear();
  pending_initializations_.clear();
  passkey_model_observation_.Reset();
}

void AffiliationsPrefetcher::DisablePrefetching() {
  // Don't do anything if prefetching was canceled already.
  if (is_prefetching_canceled_) {
    return;
  }

  is_prefetching_canceled_ = true;
  // Clear existing cache.
  affiliation_service_->KeepPrefetchForFacets({});
}

void AffiliationsPrefetcher::OnLoginsChanged(
    PasswordStoreInterface* /*store*/,
    const PasswordStoreChangeList& changes) {
  std::vector<FacetURI> facet_uris_to_trim;
  for (const PasswordStoreChange& change : changes) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(change.form().signon_realm);

    if (!facet_uri.is_valid())
      continue;

    if (!IsFacetValidForAffiliation(facet_uri)) {
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

void AffiliationsPrefetcher::OnLoginsRetained(
    PasswordStoreInterface* /*store*/,
    const std::vector<PasswordForm>& retained_passwords) {
  std::vector<FacetURI> facets;
  for (const auto& form : retained_passwords) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
    if (IsFacetValidForAffiliation(facet_uri))
      facets.push_back(std::move(facet_uri));
  }
  // TODO(crbug.com/1100818): Current logic cancels prefetch for all missing
  // facets. This might be wrong if both account and profile store is used.
  affiliation_service_->KeepPrefetchForFacets(std::move(facets));
}

void AffiliationsPrefetcher::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  std::vector<FacetURI> facet_uris_to_trim;
  for (const webauthn::PasskeyModelChange& change : changes) {
    std::optional<FacetURI> facet = FacetURIFromPasskey(change.passkey());
    if (!facet) {
      continue;
    }

    if (change.type() == webauthn::PasskeyModelChange::ChangeType::ADD) {
      affiliation_service_->Prefetch(std::move(*facet), base::Time::Max());
    } else if (change.type() ==
               webauthn::PasskeyModelChange::ChangeType::REMOVE) {
      affiliation_service_->CancelPrefetch(std::move(*facet),
                                           base::Time::Max());
    }
  }
}

void AffiliationsPrefetcher::OnPasskeyModelShuttingDown() {
  passkey_model_observation_.Reset();
}

void AffiliationsPrefetcher::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK(on_password_forms_received_barrier_callback_);
  on_password_forms_received_barrier_callback_.Run(std::move(results));
}

void AffiliationsPrefetcher::OnResultFromAllStoresReceived(
    std::vector<std::vector<std::unique_ptr<PasswordForm>>> results) {
  // If PasswordStore is registered while awaiting for results from already
  // registered PasswordStores, reinitialize it again to account newly added
  // store.
  if (!pending_initializations_.empty()) {
    InitializeWithPasswordStores();
    return;
  }

  // If no calls to Register* happened before |kInitializationDelayOnStartup|,
  // don't do anything.
  if (results.empty() && !passkey_model_observation_.IsObserving()) {
    is_ready_ = true;
    return;
  }

  std::vector<FacetURI> facets;
  for (const auto& result_per_store : results) {
    for (const auto& form : result_per_store) {
      FacetURI facet_uri =
          FacetURI::FromPotentiallyInvalidSpec(form->signon_realm);
      if (IsFacetValidForAffiliation(facet_uri)) {
        facets.push_back(std::move(facet_uri));
      }
    }
  }
  if (passkey_model_observation_.IsObserving()) {
    for (const auto& passkey :
         passkey_model_observation_.GetSource()->GetAllPasskeys()) {
      std::optional<FacetURI> facet = FacetURIFromPasskey(passkey);
      if (facet) {
        facets.push_back(std::move(*facet));
      }
    }
  }
  affiliation_service_->KeepPrefetchForFacets(facets);
  affiliation_service_->TrimUnusedCache(std::move(facets));
  is_ready_ = true;
}

void AffiliationsPrefetcher::InitializeWithPasswordStores() {
  // Don't do anything if prefetching is canceled.
  if (is_prefetching_canceled_) {
    return;
  }

  // If no calls to Register* happened before |kInitializationDelayOnStartup|
  // return early.
  if (pending_initializations_.empty() &&
      !passkey_model_observation_.IsObserving()) {
    is_ready_ = true;
    return;
  }

  is_ready_ = false;
  for (const auto& store : pending_initializations_) {
    store->AddObserver(this);
    password_stores_.push_back(store);
  }
  pending_initializations_.clear();

  on_password_forms_received_barrier_callback_ =
      base::BarrierCallback<std::vector<std::unique_ptr<PasswordForm>>>(
          password_stores_.size(),
          base::BindOnce(&AffiliationsPrefetcher::OnResultFromAllStoresReceived,
                         weak_ptr_factory_.GetWeakPtr()));
  for (const auto& store : password_stores_) {
    store->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

}  // namespace password_manager
