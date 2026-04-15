// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/indigo_internals/indigo_internals_page_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/profiles/profile.h"

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
  }
}

}  // namespace

IndigoInternalsPageHandler::IndigoInternalsPageHandler(
    mojo::PendingReceiver<indigo_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<indigo_internals::mojom::Page> page,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile) {
  indigo::IndigoService* service =
      indigo::IndigoServiceFactory::GetForProfile(profile_);
  CHECK(service);
  subscription_ =
      service->RegisterLocalEligibilityChangedCallback(base::BindRepeating(
          &IndigoInternalsPageHandler::OnLocalEligibilityChanged,
          base::Unretained(this)));
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
