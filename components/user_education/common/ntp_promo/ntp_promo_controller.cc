// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_order.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

using Eligibility = NtpPromoSpecification::Eligibility;

constexpr char kPromoMetricPrefix[] = "UserEducation.NtpPromos.Promos.";
// LINT.IfChange(NtpPromoActions)
constexpr char kPromoMetricShownSuffix[] = ".Shown";
constexpr char kPromoMetricClickedSuffix[] = ".Clicked";
constexpr char kPromoMetricCompletedSuffix[] = ".Completed";
// LINT.ThenChange(//tools/metrics/histograms/metadata/user_education/histograms.xml:NtpPromoActions)

void LogPromoMetric(const NtpPromoIdentifier& id, const std::string& suffix) {
  base::UmaHistogramBoolean(base::StrCat({kPromoMetricPrefix, id, suffix}),
                            true);
}

void LogPromoShown(const NtpPromoIdentifier& id) {
  LogPromoMetric(id, kPromoMetricShownSuffix);
}

void LogPromoClicked(const NtpPromoIdentifier& id) {
  LogPromoMetric(id, kPromoMetricClickedSuffix);
}

void LogPromoCompleted(const NtpPromoIdentifier& id) {
  LogPromoMetric(id, kPromoMetricCompletedSuffix);
}

}  // namespace

NtpPromoControllerParams GetNtpPromoControllerParams() {
  NtpPromoControllerParams params;
  params.max_top_spot_sessions =
      features::GetNtpBrowserPromoMaxTopSpotSessions();
  params.clicked_hide_duration =
      features::GetNtpBrowserPromoClickedHideDuration();
  params.promos_snoozed_hide_duration =
      features::GetNtpBrowserPromosSnoozedHideDuration();
  params.suppress_list = features::GetNtpBrowserPromoSuppressList();
  return params;
}

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

NtpPromoControllerParams::NtpPromoControllerParams() = default;
NtpPromoControllerParams::~NtpPromoControllerParams() = default;
NtpPromoControllerParams::NtpPromoControllerParams(
    const NtpPromoControllerParams&) noexcept = default;
NtpPromoControllerParams& NtpPromoControllerParams::operator=(
    NtpPromoControllerParams&&) noexcept = default;

NtpPromoController::NtpPromoController(
    NtpPromoRegistry& registry,
    UserEducationStorageService& storage_service,
    const NtpPromoControllerParams& params)
    : registry_(registry), storage_service_(storage_service), params_(params) {
  order_policy_ = std::make_unique<NtpPromoOrderPolicy>(
      registry, storage_service, params_.max_top_spot_sessions);
}

NtpPromoController::~NtpPromoController() = default;

bool NtpPromoController::HasShowablePromo(
    const user_education::UserEducationContextPtr& context) {
  // Generate promo lists here, since the Eligibility callback results are
  // insufficient. Promo callbacks may report Eligible or Completed, but promos
  // may be suppressed for a number of reasons.
  const auto promos = GenerateShowablePromo(context, /*apply_ordering=*/false);
  return !promos.empty();
}

NtpShowablePromos NtpPromoController::GenerateShowablePromo(
    const user_education::UserEducationContextPtr& context) {
  return GenerateShowablePromo(context, /*apply_ordering=*/true);
}

NtpShowablePromos NtpPromoController::GenerateShowablePromo(
    const user_education::UserEducationContextPtr& context,
    bool apply_ordering) {
  if (ArePromosBlocked()) {
    return NtpShowablePromos();
  }

  std::vector<NtpPromoIdentifier> pending_promo_ids;
  const auto now = storage_service_->GetCurrentTime();

  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    const auto* spec = registry_->GetNtpPromoSpecification(id);
    // TODO: Could this be null due to modifying Web UI state? Be tolerant?
    CHECK(spec);

    NtpPromoSpecification::Eligibility eligibility =
        spec->eligibility_callback().Run(context);
    if (eligibility == NtpPromoSpecification::Eligibility::kIneligible) {
      continue;
    }

    auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());

    // Record the first evidence of completion. In the future, promos may
    // explicitly notify of completion, but we'll also use this opportunity.
    // This was originally used to show promos in a different visual style, but
    // now it simply logs our inferred completion metric.
    if (eligibility == Eligibility::kCompleted &&
        !prefs.last_clicked.is_null() && prefs.completed.is_null()) {
      prefs.completed = now;
      storage_service_->SaveNtpPromoData(id, prefs);
      LogPromoCompleted(id);
    }

    if (ShouldShowPromo(id, prefs, eligibility, now)) {
      pending_promo_ids.push_back(id);
    }
  }

  if (apply_ordering) {
    // Even though we only show one promo, the "ordering" is necessary to
    // determine which promo to show. For example, rotation of the promos is
    // done via this ordering mechanism.
    pending_promo_ids = order_policy_->OrderPendingPromos(pending_promo_ids);
  }

  if (pending_promo_ids.empty()) {
    return NtpShowablePromos();
  }

  const auto* spec = registry_->GetNtpPromoSpecification(pending_promo_ids[0]);
  NtpShowablePromos showable_promos;
  showable_promos.promo = NtpShowablePromo(
      spec->id(), spec->content().icon_name(),
      l10n_util::GetStringUTF8(spec->content().body_text_string_id()),
      l10n_util::GetStringUTF8(spec->content().action_button_text_string_id()));

  return showable_promos;
}

void NtpPromoController::OnPromoShown(const NtpPromoIdentifier& id) {
  const auto* spec = registry_->GetNtpPromoSpecification(id);
  spec->show_callback().Run();

  LogPromoShown(id);

  // This logic keeps track of when this promo was shown, so that it can be
  // rotated out after a period of time. The stored data is referred to as
  // "top spot" because at one point multiple promos could be shown, and the
  // top position rotated. Now that we only show one promo, it's effectively
  // the same, with the "top spot" being the only promo shown.
  const int current_session = storage_service_->GetSessionNumber();
  auto data = storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
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

void NtpPromoController::OnPromoClicked(
    NtpPromoIdentifier id,
    const user_education::UserEducationContextPtr& context) {
  registry_->GetNtpPromoSpecification(id)->action_callback().Run(context);

  auto prefs = storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
  prefs.last_clicked = storage_service_->GetCurrentTime();
  storage_service_->SaveNtpPromoData(id, prefs);
  LogPromoClicked(id);
}

void NtpPromoController::SetAllPromosSnoozed(bool snooze) {
  NtpPromoPreferences prefs = storage_service_->ReadNtpPromoPreferences();
  prefs.last_snoozed =
      snooze ? storage_service_->GetCurrentTime() : base::Time();
  storage_service_->SaveNtpPromoPreferences(prefs);
}

void NtpPromoController::SetAllPromosDisabled(bool disabled) {
  NtpPromoPreferences prefs = storage_service_->ReadNtpPromoPreferences();
  prefs.last_snoozed = base::Time();
  prefs.disabled = disabled;
  storage_service_->SaveNtpPromoPreferences(prefs);
}

NtpPromoIdentifier NtpPromoController::GetMostRecentTopSpotPromo() {
  int most_recent_session = 0;
  NtpPromoIdentifier most_recent_id;
  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
    if (prefs.last_top_spot_session > most_recent_session) {
      most_recent_session = prefs.last_top_spot_session;
      most_recent_id = id;
    }
  }
  return most_recent_id;
}

bool NtpPromoController::ArePromosBlocked() const {
  NtpPromoPreferences prefs = storage_service_->ReadNtpPromoPreferences();
  return prefs.disabled ||
         (!prefs.last_snoozed.is_null() &&
          storage_service_->GetCurrentTime() <
              prefs.last_snoozed + params_.promos_snoozed_hide_duration);
}

// Decides whether a promo should be shown or not, based on the supplied
// data. If this logic becomes more complex, consider pulling it out to a
// separate file (crbug.com/435159508).
bool NtpPromoController::ShouldShowPromo(const NtpPromoIdentifier& id,
                                         const NtpPromoData& prefs,
                                         Eligibility eligibility,
                                         const base::Time& now) {
  // If an eligible promo has been clicked recently, don't show it again for
  // a period of time.
  if (eligibility == Eligibility::kEligible && !prefs.last_clicked.is_null() &&
      ((now - prefs.last_clicked) < params_.clicked_hide_duration)) {
    return false;
  }

  // If the promo reports itself as completed, or at some point we detected it
  // was completed, don't show the promo. For example, if we inferred that
  // a user signed in as a result of that promo, but signed back out, we don't
  // re-show the promo.
  if (eligibility == Eligibility::kCompleted || !prefs.completed.is_null()) {
    return false;
  }

  // If the promo is suppressed via Finch, don't show it (ie. a kill switch).
  if (std::ranges::contains(params_.suppress_list, id)) {
    return false;
  }

  return true;
}

}  // namespace user_education
