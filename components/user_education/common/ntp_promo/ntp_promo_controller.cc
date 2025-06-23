// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "components/user_education/common/user_education_data.h"
// #include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

NtpShowablePromos::NtpShowablePromos() = default;
NtpShowablePromos::~NtpShowablePromos() = default;
NtpShowablePromos::NtpShowablePromos(NtpShowablePromos&&) noexcept = default;
NtpShowablePromos& NtpShowablePromos::operator=(NtpShowablePromos&&) noexcept =
    default;

NtpShowablePromos::Promo::Promo(NtpPromoIdentifier id,
                                const NtpPromoContent& content)
    : id(id), content(content) {}

NtpPromoController::NtpPromoController(
    NtpPromoRegistry& registry,
    UserEducationStorageService& storage_service)
    : registry_(registry), storage_service_(storage_service) {}

NtpPromoController::~NtpPromoController() = default;

NtpShowablePromos NtpPromoController::GetShowablePromos() {
  NtpShowablePromos showable_promos;

  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    const auto* spec = registry_->GetNtpPromoSpecification(id);
    // TODO: Could this be null due to modifying Web UI state? Be tolerant?
    CHECK(spec);

    NtpPromoSpecification::Eligibility eligibility =
        spec->eligibility_callback().Run(nullptr);
    if (eligibility == NtpPromoSpecification::Eligibility::kIneligible) {
      continue;
    }

    bool completed =
        (eligibility == NtpPromoSpecification::Eligibility::kCompleted);
    if (!completed) {
      // If the promo has ever been completed in the past, considered it
      // complete even if it's reverted to eligible state.
      // TODO(crbug.com/425677412): Show only for a period of time after
      // completion.
      const auto prefs = storage_service_->ReadNtpPromoData(id);
      completed = (prefs.has_value() && !prefs.value().completed.is_null());
    }

    (completed ? showable_promos.completed : showable_promos.pending)
        .emplace_back(id, spec->content());

    // TODO(crbug.com/425677412): Store completed state if observed here, in
    // lieu of explicit signals from the promo flows.
  }

  return showable_promos;
}

void NtpPromoController::OnPromoClicked(NtpPromoIdentifier id) {
  registry_->GetNtpPromoSpecification(id)->action_callback().Run(nullptr);
}

}  // namespace user_education
