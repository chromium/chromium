// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/time/time.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
namespace user_education {

namespace {

constexpr base::TimeDelta kCompletedPromoShowDuration = base::Days(7);

}  // namespace

using KeyedNtpPromoData = user_education::KeyedNtpPromoData;

using Eligibility = NtpPromoSpecification::Eligibility;

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

NtpShowablePromos NtpPromoController::GenerateShowablePromos() {
  NtpShowablePromos showable_promos;
  const auto now = base::Time::Now();

  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    const auto* spec = registry_->GetNtpPromoSpecification(id);
    // TODO: Could this be null due to modifying Web UI state? Be tolerant?
    CHECK(spec);

    NtpPromoSpecification::Eligibility eligibility =
        spec->eligibility_callback().Run(nullptr);
    if (eligibility == NtpPromoSpecification::Eligibility::kIneligible) {
      continue;
    }

    auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(KeyedNtpPromoData());

    // Record the first evidence of completion. In the future, promos may
    // explicitly notify of completion, but we'll also use this opportunity.
    if (eligibility == Eligibility::kCompleted && prefs.completed.is_null()) {
      prefs.completed = now;
      storage_service_->SaveNtpPromoData(id, prefs);
    }

    // If the promo was completed sufficiently long ago, don't show it.
    // Likewise if the completion time is nonsense (in the future).
    if (!prefs.completed.is_null() &&
        ((now - prefs.completed >= kCompletedPromoShowDuration) ||
         (now < prefs.completed))) {
      continue;
    }

    (prefs.completed.is_null() ? showable_promos.pending
                               : showable_promos.completed)
        .emplace_back(id, spec->content());
  }

  return showable_promos;
}

void NtpPromoController::OnPromoClicked(NtpPromoIdentifier id) {
  registry_->GetNtpPromoSpecification(id)->action_callback().Run(nullptr);
}

base::TimeDelta NtpPromoController::GetCompletedPromoShowDurationForTest()
    const {
  return kCompletedPromoShowDuration;
}

}  // namespace user_education
