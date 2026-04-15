// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include <algorithm>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/l10n/l10n_util.h"

// The mechanism by which this controller operates is generally as follows:
//
// The controller needs to pick which promo to show, based on the chosen
// rotation mechanism.  The rotation shows a particular promo through a number
// of user browsing sessions, unless it is clicked or dismissed.  This set of
// sessions will be referred to here as a "term".  If untouched, that promo is
// hidden for a period of time (eg. 3 months), then shown again in a new term.
// Promos are capped at a fixed number of terms.  If a promo is clicked or
// dismissed, another promo is shown in its place.
//
// The controller may be asked for a promo to show, but there is no guarantee
// the NTP actually shows that promo.
//
// To make this as simple as possible, the mechanism works as follows:
//
// To determine which promo to show, the controller iteratess through promos,
// and inspect information it has about previous showings to determine which
// one to show next.  First, it determines whether a promo *can* be shown,
// using information about when it was previously shown, and how often.  Next,
// it assesses which of those promos should actually be displayed, by selecting
// either the currently-showing promo, or if that one has exhausted its session
// count, the promo that hasn't shown in the longest period of time.  It does
// this without mutating state, such that it should return the same result in
// repeated requests, assuming no other input.
//
// Separately, the controller is called to notify that a particular promo has
// been shown on the NTP.  This is where promo history is recorded.  Session
// and term counts are updated, and timestamps are captured where needed.
// The information stored "what happened", and should be free of any decision
// logic about what should show next.  This separation aims to keep the
// system as simple and reliable as possible.

namespace user_education {

namespace {

using Eligibility = NtpPromoSpecification::Eligibility;

constexpr char kPromoMetricPrefix[] = "UserEducation.NtpPromos.Promos.";
// LINT.IfChange(NtpPromoActions)
constexpr char kPromoMetricShownSuffix[] = ".Shown";
constexpr char kPromoMetricClickedSuffix[] = ".Clicked";
constexpr char kPromoMetricCompletedSuffix[] = ".Completed";
constexpr char kPromoMetricDismissedSuffix[] = ".Dismissed";
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

void LogPromoDismissed(const NtpPromoIdentifier& id) {
  LogPromoMetric(id, kPromoMetricDismissedSuffix);
}

}  // namespace

NtpPromoControllerParams GetNtpPromoControllerParams() {
  NtpPromoControllerParams params;
  params.max_sessions_per_term =
      features::GetNtpBrowserPromoMaxSessionsPerTerm();
  params.max_terms = features::GetNtpBrowserPromoMaxTerms();
  params.cool_off_duration = features::GetNtpBrowserPromoClickedHideDuration();
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
    : registry_(registry), storage_service_(storage_service), params_(params) {}

NtpPromoController::~NtpPromoController() = default;

bool NtpPromoController::HasShowablePromo(
    const user_education::UserEducationContextPtr& context) {
  const auto promo = GenerateShowablePromo(context);
  return promo.has_value();
}

// See the full explanation of how this system works at the top of this file.
std::optional<NtpShowablePromo> NtpPromoController::GenerateShowablePromo(
    const user_education::UserEducationContextPtr& context) {
  if (ArePromosBlocked()) {
    return std::nullopt;
  }

  std::vector<NtpPromoIdentifier> pending_promo_ids;
  const auto now = storage_service_->GetCurrentTime();

  NtpPromoIdentifier selected_promo_id;
  int oldest_session = std::numeric_limits<int>::max();
  const int current_session = storage_service_->GetSessionNumber();
  NtpPromoIdentifier most_recent_promo = GetMostRecentTopSpotPromo();
  NtpPromoIdentifier promo_shown_this_session;
  if (!most_recent_promo.empty()) {
    auto most_recent_prefs =
        storage_service_->ReadNtpPromoData(most_recent_promo)
            .value_or(NtpPromoData());
    if (most_recent_prefs.last_session == current_session) {
      promo_shown_this_session = most_recent_promo;
    }
  }

  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    const auto* spec = registry_->GetNtpPromoSpecification(id);
    // TODO: Could this be null due to modifying Web UI state? Be tolerant?
    CHECK(spec);

    NtpPromoSpecification::Eligibility eligibility =
        spec->eligibility_callback().Run(context);

    // Record the first evidence of completion. In the future, promos may
    // explicitly notify of completion, but we'll also use this opportunity.
    // This was originally used to show promos in a different visual style, but
    // now it simply logs our inferred completion metric.
    auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
    if (eligibility == Eligibility::kCompleted &&
        !prefs.last_clicked.is_null() && prefs.completed.is_null()) {
      prefs.completed = now;
      storage_service_->SaveNtpPromoData(id, prefs);
      LogPromoCompleted(id);
    }

    if (CanShowPromo(id, prefs, eligibility, now, current_session)) {
      // This promo is able to be shown. Next, decide if it's actually the one
      // we want to show or not.

      // If we already showed a promo this session, and this wasn't it, skip it.
      // If a promo is shown then dismissed for any reason, we won't show
      // another promo until the next session.
      if (!promo_shown_this_session.empty() && id != promo_shown_this_session) {
        continue;
      }

      if (prefs.last_session == current_session ||
          (id == most_recent_promo &&
           prefs.session_count_in_term < params_.max_sessions_per_term)) {
        // This promo is currently showing in this session, or is continuing
        // its term across a session boundary. It must keep showing.
        selected_promo_id = id;
        break;
      }

      if (prefs.last_session < oldest_session) {
        // Fallback: keep track of the least recently shown eligible promo.
        // This is the one to be shown if the previously-shown promo has
        // reached its impression limit.
        oldest_session = prefs.last_session;
        selected_promo_id = id;
      }
    }
  }

  if (selected_promo_id.empty()) {
    return std::nullopt;
  }

  const auto* spec = registry_->GetNtpPromoSpecification(selected_promo_id);
  return NtpShowablePromo(
      spec->id(), spec->content().icon_name(),
      l10n_util::GetStringUTF8(spec->content().body_text_string_id()),
      l10n_util::GetStringUTF8(spec->content().action_button_text_string_id()));
}

void NtpPromoController::OnPromoShown(const NtpPromoIdentifier& id) {
  const auto* spec = registry_->GetNtpPromoSpecification(id);
  spec->show_callback().Run();

  LogPromoShown(id);

  // This logic keeps track of when this promo was shown, so that it can be
  // rotated out after a period of time.
  const int current_session = storage_service_->GetSessionNumber();
  auto data = storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());

  if (data.last_session != current_session) {
    // If this promo is reclaiming the top spot, or starting a new term, start
    // a fresh count.
    if (id != GetMostRecentTopSpotPromo()) {
      data.session_count_in_term = 0;
      data.term_count++;
      data.term_start_time = storage_service_->GetCurrentTime();
    }
    data.last_session = current_session;
    data.session_count_in_term++;
  }

  storage_service_->SaveNtpPromoData(id, data);
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

void NtpPromoController::SetAllPromosDisabled(bool disabled) {
  NtpPromoPreferences prefs = storage_service_->ReadNtpPromoPreferences();
  prefs.disabled = disabled;
  storage_service_->SaveNtpPromoPreferences(prefs);
}

void NtpPromoController::OnPromoDismissed(const NtpPromoIdentifier& id) {
  auto prefs = storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
  prefs.dismissed_time = storage_service_->GetCurrentTime();
  storage_service_->SaveNtpPromoData(id, prefs);
  LogPromoDismissed(id);
}

NtpPromoIdentifier NtpPromoController::GetMostRecentTopSpotPromo() {
  int most_recent_session = 0;
  NtpPromoIdentifier most_recent_id;
  for (const auto& id : registry_->GetNtpPromoIdentifiers()) {
    auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
    if (prefs.last_session > most_recent_session) {
      most_recent_session = prefs.last_session;
      most_recent_id = id;
    }
  }
  return most_recent_id;
}

bool NtpPromoController::ArePromosBlocked() const {
  NtpPromoPreferences prefs = storage_service_->ReadNtpPromoPreferences();
  return prefs.disabled;
}

// See the full explanation of how this system works at the top of this file.
bool NtpPromoController::CanShowPromo(const NtpPromoIdentifier& id,
                                      const NtpPromoData& prefs,
                                      Eligibility eligibility,
                                      const base::Time& now,
                                      const int current_session) {
  if (eligibility != NtpPromoSpecification::Eligibility::kEligible) {
    return false;
  }

  // If we ever observed the promo to be completed, don't show the promo. For
  // example, if we inferred that a user signed in as a result of a promo,
  // but signed back out, we don't re-show the promo.
  if (!prefs.completed.is_null()) {
    return false;
  }

  if (!prefs.dismissed_time.is_null()) {
    return false;
  }

  // If a promo was clicked, don't show it again until after the cool-down
  // period.
  if (!prefs.last_clicked.is_null() &&
      ((now - prefs.last_clicked) < params_.cool_off_duration)) {
    return false;
  }

  // If the promo is suppressed via Finch, don't show it (ie. a kill switch).
  if (std::ranges::contains(params_.suppress_list, id)) {
    return false;
  }

  if (current_session > prefs.last_session &&
      prefs.session_count_in_term >= params_.max_sessions_per_term &&
      prefs.term_count >= params_.max_terms) {
    return false;
  }

  if (current_session > prefs.last_session &&
      prefs.session_count_in_term >= params_.max_sessions_per_term &&
      !prefs.term_start_time.is_null() &&
      ((now - prefs.term_start_time) < params_.cool_off_duration)) {
    return false;
  }

  return true;
}

}  // namespace user_education
