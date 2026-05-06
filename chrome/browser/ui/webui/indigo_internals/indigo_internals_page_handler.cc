// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/indigo_internals/indigo_internals_page_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

namespace {

indigo_internals::mojom::LocalEligibility MapLocalEligibility(
    indigo::LocalEligibility status) {
  switch (status) {
    case indigo::LocalEligibility::kEligible:
      return indigo_internals::mojom::LocalEligibility::kEligible;
    case indigo::LocalEligibility::kNotSignedIn:
      return indigo_internals::mojom::LocalEligibility::kNotSignedIn;
    case indigo::LocalEligibility::kMissingCapabilities:
      return indigo_internals::mojom::LocalEligibility::kMissingCapabilities;
    case indigo::LocalEligibility::kDisabledByPolicy:
      return indigo_internals::mojom::LocalEligibility::kDisabledByPolicy;
    case indigo::LocalEligibility::kMissingScript:
      return indigo_internals::mojom::LocalEligibility::kMissingScript;
  }
}

indigo_internals::mojom::OptimizationGuideStatus
GetCurrentOptimizationGuideStatus(Profile* profile) {
  if (!optimization_guide::features::IsOptimizationHintsEnabled()) {
    return indigo_internals::mojom::OptimizationGuideStatus::kDisabled;
  }
  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), profile->GetPrefs())) {
    return indigo_internals::mojom::OptimizationGuideStatus::kNotPermitted;
  }
  return indigo_internals::mojom::OptimizationGuideStatus::kEnabled;
}

}  // namespace

IndigoInternalsPageHandler::IndigoInternalsPageHandler(
    mojo::PendingReceiver<indigo_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<indigo_internals::mojom::Page> page,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile),
      consent_helper_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs())) {
  indigo::IndigoService* service =
      indigo::IndigoServiceFactory::GetForProfile(profile_);
  CHECK(service);
  subscription_ =
      service->RegisterLocalEligibilityChangedCallback(base::BindRepeating(
          &IndigoInternalsPageHandler::OnLocalEligibilityChanged,
          base::Unretained(this)));
  consent_observation_.Observe(consent_helper_.get());
}

IndigoInternalsPageHandler::~IndigoInternalsPageHandler() = default;

void IndigoInternalsPageHandler::GetLocalEligibility(
    GetLocalEligibilityCallback callback) {
  indigo::IndigoService* service =
      indigo::IndigoServiceFactory::GetForProfile(profile_);
  CHECK(service);

  std::move(callback).Run(MapLocalEligibility(service->GetLocalEligibility()));
}

void IndigoInternalsPageHandler::OnLocalEligibilityChanged(
    indigo::LocalEligibility status) {
  page_->OnLocalEligibilityChanged(MapLocalEligibility(status));
}

void IndigoInternalsPageHandler::GetCombinedEligibility(
    GetCombinedEligibilityCallback callback) {
  indigo::IndigoService* service =
      indigo::IndigoServiceFactory::GetForProfile(profile_);
  CHECK(service);

  service->GetCombinedEligibility(base::BindOnce(
      [](GetCombinedEligibilityCallback callback,
         const indigo::CombinedEligibility& eligibility) {
        auto mojo_eligibility =
            indigo_internals::mojom::CombinedEligibility::New();
        mojo_eligibility->local_eligibility =
            MapLocalEligibility(eligibility.local_eligibility);

        if (eligibility.remote_eligibility.has_value()) {
          auto remote_mojo = indigo_internals::mojom::RemoteEligibility::New();
          remote_mojo->is_service_supported_for_account =
              eligibility.remote_eligibility->is_service_supported_for_account;
          remote_mojo->has_user_image =
              eligibility.remote_eligibility->has_user_image;
          mojo_eligibility->remote_eligibility = std::move(remote_mojo);
        }

        mojo_eligibility->can_generate_image = eligibility.CanGenerateImage();
        mojo_eligibility->ready_to_onboard = eligibility.ReadyToOnboard();
        mojo_eligibility->has_onboarded_pref = eligibility.has_onboarded_pref;
        std::move(callback).Run(std::move(mojo_eligibility));
      },
      std::move(callback)));
}
void IndigoInternalsPageHandler::InvalidateRemoteEligibility() {
  indigo::IndigoService* service =
      indigo::IndigoServiceFactory::GetForProfile(profile_);
  CHECK(service);
  service->InvalidateRemoteEligibility();
}

void IndigoInternalsPageHandler::GetOptimizationGuideStatus(
    GetOptimizationGuideStatusCallback callback) {
  std::move(callback).Run(GetCurrentOptimizationGuideStatus(profile_));
}

void IndigoInternalsPageHandler::OnUrlKeyedDataCollectionConsentStateChanged(
    unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper) {
  page_->OnOptimizationGuideStatusChanged(
      GetCurrentOptimizationGuideStatus(profile_));
}
