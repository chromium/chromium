// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/time/time.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

constexpr base::TimeDelta kCompletedPromoShowDuration = base::Days(7);

}  // namespace

using Eligibility = NtpPromoSpecification::Eligibility;

NtpShowablePromo::NtpShowablePromo() = default;
NtpShowablePromo::NtpShowablePromo(std::string_view id_,
                                   std::string_view icon_name_,
                                   std::string_view body_text_,
                                   std::string_view action_button_text_)
    : id(id_),
      icon_name(icon_name_),
      body_text(body_text_),
      action_button_text(action_button_text_) {}
NtpShowablePromo::NtpShowablePromo(const NtpShowablePromo& other) = default;
NtpShowablePromo& NtpShowablePromo::operator=(const NtpShowablePromo& other) =
    default;
NtpShowablePromo::~NtpShowablePromo() = default;

NtpShowablePromos::NtpShowablePromos() = default;
NtpShowablePromos::~NtpShowablePromos() = default;
NtpShowablePromos::NtpShowablePromos(NtpShowablePromos&&) noexcept = default;
NtpShowablePromos& NtpShowablePromos::operator=(NtpShowablePromos&&) noexcept =
    default;

NtpPromoController::NtpPromoController(
    NtpPromoRegistry& registry,
    UserEducationStorageService& storage_service)
    : registry_(registry), storage_service_(storage_service) {}

NtpPromoController::~NtpPromoController() = default;

bool NtpPromoController::HasShowablePromos(Profile* profile) const {
  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    if (const auto* spec = registry_->GetNtpPromoSpecification(id)) {
      if (spec->eligibility_callback().Run(profile) !=
          NtpPromoSpecification::Eligibility::kIneligible) {
        return true;
      }
    }
  }
  return false;
}

NtpShowablePromos NtpPromoController::GenerateShowablePromos(Profile* profile) {
  NtpShowablePromos showable_promos;
  const auto now = base::Time::Now();

  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    const auto* spec = registry_->GetNtpPromoSpecification(id);
    // TODO: Could this be null due to modifying Web UI state? Be tolerant?
    CHECK(spec);

    NtpPromoSpecification::Eligibility eligibility =
        spec->eligibility_callback().Run(profile);
    if (eligibility == NtpPromoSpecification::Eligibility::kIneligible) {
      continue;
    }

    auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(KeyedNtpPromoData());

    // If the promo reports itself as complete, but was never invoked by the
    // user, don't show it (eg. user is already signed in).
    if (eligibility == Eligibility::kCompleted &&
        prefs.last_clicked.is_null()) {
      continue;
    }

    // Record the first evidence of completion. In the future, promos may
    // explicitly notify of completion, but we'll also use this opportunity.
    if (eligibility == Eligibility::kCompleted &&
        !prefs.last_clicked.is_null() && prefs.completed.is_null()) {
      prefs.completed = now;
      storage_service_->SaveNtpPromoData(id, prefs);
    }

    // If the promo was marked complete sufficiently long ago, don't show it.
    // Likewise if the completion time is nonsense (in the future).
    if (!prefs.completed.is_null() &&
        ((now - prefs.completed >= kCompletedPromoShowDuration) ||
         (now < prefs.completed))) {
      continue;
    }

    NtpShowablePromo promo(
        spec->id(), spec->content().icon_name(),
        l10n_util::GetStringUTF8(spec->content().body_text_string_id()),
        l10n_util::GetStringUTF8(
            spec->content().action_button_text_string_id()));
    (prefs.completed.is_null() ? showable_promos.pending
                               : showable_promos.completed)
        .push_back(std::move(promo));
  }

  return showable_promos;
}

void NtpPromoController::OnPromosShown(
    const std::vector<NtpPromoIdentifier>& eligible_shown,
    const std::vector<NtpPromoIdentifier>& completed_shown) {
  // In the current implementation, only the top eligible promo needs to be
  // updated. However, metrics should be output for every promo shown in this
  // way.
  if (!eligible_shown.empty()) {
    OnPromoShownInTopSpot(eligible_shown[0]);
  }
}

void NtpPromoController::OnPromoClicked(NtpPromoIdentifier id,
                                        BrowserWindowInterface* browser) {
  registry_->GetNtpPromoSpecification(id)->action_callback().Run(browser);

  auto prefs =
      storage_service_->ReadNtpPromoData(id).value_or(KeyedNtpPromoData());
  prefs.last_clicked = base::Time::Now();
  storage_service_->SaveNtpPromoData(id, prefs);
}

// static
base::TimeDelta NtpPromoController::GetCompletedPromoShowDurationForTest() {
  return kCompletedPromoShowDuration;
}

void NtpPromoController::OnPromoShownInTopSpot(NtpPromoIdentifier id) {
  const int current_session = storage_service_->GetSessionNumber();
  // If no data is present, default-construct.
  auto data =
      storage_service_->ReadNtpPromoData(id).value_or(KeyedNtpPromoData());
  if (data.last_top_spot_session != current_session) {
    data.last_top_spot_session = current_session;
    ++data.top_spot_session_count;
    storage_service_->SaveNtpPromoData(id, data);
  }
}

}  // namespace user_education
