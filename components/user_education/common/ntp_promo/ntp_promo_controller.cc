// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/time/time.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_order.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

using Eligibility = NtpPromoSpecification::Eligibility;

constexpr int kNumSessionsBetweenTopPromoRotation = 3;
constexpr base::TimeDelta kCompletedPromoShowDuration = base::Days(7);
constexpr base::TimeDelta kClickedPromoHideDuration = base::Days(90);

// Decides whether a promo should be shown or not, based on the supplied
// data. If this logic becomes more complex, consider pulling it out to a
// separate file (crbug.com/435159508).
bool ShouldShowPromo(const KeyedNtpPromoData& prefs,
                     Eligibility eligibility,
                     const base::Time& now) {
  // If an eligible promo has been clicked recently, don't show it again for
  // a period of time.
  if (eligibility == Eligibility::kEligible && !prefs.last_clicked.is_null() &&
      ((now - prefs.last_clicked) < kClickedPromoHideDuration)) {
    return false;
  }

  // If the promo reports itself as complete, but was never invoked by the
  // user, don't show it (eg. user is already signed in).
  if (eligibility == Eligibility::kCompleted && prefs.last_clicked.is_null()) {
    return false;
  }

  // If the promo was marked complete sufficiently long ago, don't show it.
  // Likewise if the completion time is nonsense (in the future).
  if (!prefs.completed.is_null() &&
      ((now - prefs.completed >= kCompletedPromoShowDuration) ||
       (now < prefs.completed))) {
    return false;
  }

  return true;
}

}  // namespace

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
    : registry_(registry), storage_service_(storage_service) {
  // TODO(crbug.com/421398754): Allow Finch to override ordering criteria.
  order_policy_ = std::make_unique<NtpPromoOrderPolicy>(
      registry, storage_service, kNumSessionsBetweenTopPromoRotation);
}

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
  std::vector<NtpPromoIdentifier> pending_promo_ids;
  std::vector<NtpPromoIdentifier> completed_promo_ids;
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

    // Record the first evidence of completion. In the future, promos may
    // explicitly notify of completion, but we'll also use this opportunity.
    if (eligibility == Eligibility::kCompleted &&
        !prefs.last_clicked.is_null() && prefs.completed.is_null()) {
      prefs.completed = now;
      storage_service_->SaveNtpPromoData(id, prefs);
    }

    if (!ShouldShowPromo(prefs, eligibility, now)) {
      continue;
    }

    (prefs.completed.is_null() ? pending_promo_ids : completed_promo_ids)
        .push_back(id);
  }

  pending_promo_ids = order_policy_->OrderPendingPromos(pending_promo_ids);
  completed_promo_ids =
      order_policy_->OrderCompletedPromos(completed_promo_ids);

  NtpShowablePromos showable_promos;
  showable_promos.pending = MakeShowablePromos(pending_promo_ids);
  showable_promos.completed = MakeShowablePromos(completed_promo_ids);
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

    for (const auto& id : eligible_shown) {
      const auto* spec = registry_->GetNtpPromoSpecification(id);
      spec->show_callback().Run();
    }
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

// static
base::TimeDelta NtpPromoController::GetClickedPromoHideDurationForTest() {
  return kClickedPromoHideDuration;
}

void NtpPromoController::OnPromoShownInTopSpot(NtpPromoIdentifier id) {
  const int current_session = storage_service_->GetSessionNumber();
  // If no data is present, default-construct.
  auto data =
      storage_service_->ReadNtpPromoData(id).value_or(KeyedNtpPromoData());
  if (data.last_top_spot_session != current_session) {
    data.last_top_spot_session = current_session;
    // If this promo is reclaiming the top spot, start a fresh count.
    if (id != GetMostRecentTopSpotPromo()) {
      data.top_spot_session_count = 0;
    }
    data.top_spot_session_count++;
    storage_service_->SaveNtpPromoData(id, data);
  }
}

std::vector<NtpShowablePromo> NtpPromoController::MakeShowablePromos(
    const std::vector<NtpPromoIdentifier>& ids) {
  std::vector<NtpShowablePromo> promos;
  for (const auto& id : ids) {
    const auto* spec = registry_->GetNtpPromoSpecification(id);
    promos.emplace_back(
        spec->id(), spec->content().icon_name(),
        l10n_util::GetStringUTF8(spec->content().body_text_string_id()),
        l10n_util::GetStringUTF8(
            spec->content().action_button_text_string_id()));
  }
  return promos;
}

NtpPromoIdentifier NtpPromoController::GetMostRecentTopSpotPromo() {
  int most_recent_session = 0;
  NtpPromoIdentifier most_recent_id;
  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(KeyedNtpPromoData());
    if (prefs.last_top_spot_session > most_recent_session) {
      most_recent_session = prefs.last_top_spot_session;
      most_recent_id = id;
    }
  }
  return most_recent_id;
}

}  // namespace user_education
