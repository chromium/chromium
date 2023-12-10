// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_lifecycle.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/user_education_features.h"

namespace user_education {

namespace {

class ScopedPromoData {
 public:
  ScopedPromoData(FeaturePromoStorageService* storage_service,
                  const base::Feature* iph_feature)
      : storage_service_(storage_service), iph_feature_(iph_feature) {
    promo_data_ = storage_service_->ReadPromoData(*iph_feature)
                      .value_or(FeaturePromoData());
  }

  ~ScopedPromoData() {
    storage_service_->SavePromoData(*iph_feature_, promo_data_);
  }

  FeaturePromoData* operator->() { return &promo_data_; }

 private:
  FeaturePromoData promo_data_;
  const raw_ptr<FeaturePromoStorageService> storage_service_;
  const raw_ptr<const base::Feature> iph_feature_;
};

}  // namespace

FeaturePromoLifecycle::FeaturePromoLifecycle(
    FeaturePromoStorageService* storage_service,
    const base::StringPiece& app_id,
    const base::Feature* iph_feature,
    PromoType promo_type,
    PromoSubtype promo_subtype)
    : storage_service_(storage_service),
      app_id_(app_id),
      iph_feature_(iph_feature),
      promo_type_(promo_type),
      promo_subtype_(promo_subtype) {}

FeaturePromoLifecycle::~FeaturePromoLifecycle() {
  MaybeEndPromo();
}

FeaturePromoResult FeaturePromoLifecycle::CanShow() const {
  DCHECK(promo_subtype_ != PromoSubtype::kPerApp || !app_id_.empty());

  const auto data = storage_service_->ReadPromoData(*iph_feature_);
  if (!data.has_value()) {
    return FeaturePromoResult::Success();
  }

  switch (promo_subtype_) {
    case PromoSubtype::kNormal:
      switch (promo_type_) {
        case PromoType::kLegacy:
        case PromoType::kToast:
          return FeaturePromoResult::Success();
        case PromoType::kCustomAction:
        case PromoType::kSnooze:
        case PromoType::kTutorial:
          return CanShowSnoozePromo(*data);
        case PromoType::kUnspecified:
          NOTREACHED();
          return FeaturePromoResult::kPermanentlyDismissed;
      }
      break;
    case PromoSubtype::kPerApp:
      return base::Contains(data->shown_for_apps, app_id_)
                 ? FeaturePromoResult::kPermanentlyDismissed
                 : FeaturePromoResult::Success();
    case PromoSubtype::kLegalNotice:
      return data->is_dismissed ? FeaturePromoResult::kPermanentlyDismissed
                                : FeaturePromoResult::Success();
  }
}

bool FeaturePromoLifecycle::CanSnooze() const {
  switch (promo_type_) {
    case PromoType::kLegacy:
    case PromoType::kToast:
      return false;
    case PromoType::kCustomAction:
    case PromoType::kSnooze:
    case PromoType::kTutorial:
      // Only enforce snooze count in V2 to avoid breaking backwards behavior.
      return !features::IsUserEducationV2() ||
             storage_service_->GetSnoozeCount(*iph_feature_) <
                 features::GetMaxSnoozeCount();
    case PromoType::kUnspecified:
      NOTREACHED();
      return false;
  }
}

void FeaturePromoLifecycle::OnPromoShown(
    std::unique_ptr<HelpBubble> help_bubble,
    feature_engagement::Tracker* tracker) {
  CHECK(!was_started());
  state_ = State::kRunning;
  tracker_ = tracker;
  help_bubble_ = std::move(help_bubble);
  if (is_demo()) {
    return;
  }
  ScopedPromoData data(storage_service_, iph_feature_);
  const auto now = GetCurrentTime();
  if (data->show_count == 0) {
    data->first_show_time = now;
  }
  ++data->show_count;
  data->last_show_time = now;
  RecordShown();
}

void FeaturePromoLifecycle::OnPromoShownForDemo(
    std::unique_ptr<HelpBubble> help_bubble) {
  OnPromoShown(std::move(help_bubble), nullptr);
}

bool FeaturePromoLifecycle::OnPromoBubbleClosed() {
  help_bubble_.reset();
  if (state_ == State::kRunning) {
    MaybeRecordClosedReason(FeaturePromoClosedReason::kAbortPromo);
    CHECK(MaybeEndPromo());
    return true;
  }
  return false;
}

void FeaturePromoLifecycle::OnPromoEnded(FeaturePromoClosedReason close_reason,
                                         bool continue_promo) {
  MaybeRecordClosedReason(close_reason);
  if (continue_promo) {
    CHECK(is_bubble_visible());
    state_ = State::kContinued;
    // When a snoozeable, normal promo that has a follow-up action (Tutorial,
    // custom action), the result is not recorded until after the follow-up
    // finishes, because e.g. an aborted tutorial counts as a snooze.
    if (promo_subtype_ != PromoSubtype::kNormal ||
        close_reason != FeaturePromoClosedReason::kAction) {
      MaybeWriteClosedPromoData(close_reason);
    }
    help_bubble_->Close();
  } else {
    CHECK(MaybeEndPromo());
    MaybeWriteClosedPromoData(close_reason);
  }
}

void FeaturePromoLifecycle::OnContinuedPromoEnded(bool completed_successfully) {
  MaybeWriteClosedPromoData(completed_successfully
                                ? FeaturePromoClosedReason::kAction
                                : FeaturePromoClosedReason::kSnooze);
  MaybeEndPromo();
}

bool FeaturePromoLifecycle::MaybeEndPromo() {
  if (!is_promo_active()) {
    return false;
  }
  state_ = State::kClosed;
  if (!is_demo() && !tracker_dismissed_) {
    tracker_dismissed_ = true;
    tracker_->Dismissed(*iph_feature_);
  }
  return true;
}

FeaturePromoResult FeaturePromoLifecycle::CanShowSnoozePromo(
    const FeaturePromoData& promo_data) const {
  // This IPH has been dismissed by user permanently.
  if (promo_data.is_dismissed) {
    return FeaturePromoResult::kPermanentlyDismissed;
  }

  // This IPH is shown for the first time.
  if (promo_data.show_count == 0) {
    return FeaturePromoResult::Success();
  }

  const auto now = GetCurrentTime();

  // Figure out when the promo can show next.
  if (features::IsUserEducationV2()) {
    // In V2, there is a separate cooldown if a promo is snoozed vs. shown but
    // not snoozed, for example, if it was aborted for some other reason and not
    // dismissed.
    if (now < promo_data.last_snooze_time + features::GetSnoozeDuration()) {
      return FeaturePromoResult::kSnoozed;
    }
    if (now < promo_data.last_show_time + features::GetAbortCooldown()) {
      return FeaturePromoResult::kRecentlyAborted;
    }
  } else {
    // In V1, it was always the default snooze duration from the previous
    // show or snooze time (non-snoozed IPH were subject to "non-clicker policy"
    // which still used the default snooze duration).
    const auto last_show =
        std::max(promo_data.last_show_time, promo_data.last_snooze_time);
    if (now < last_show + features::GetSnoozeDuration()) {
      return FeaturePromoResult::kSnoozed;
    }
  }

  return FeaturePromoResult::Success();
}

base::Time FeaturePromoLifecycle::GetCurrentTime() const {
  return storage_service_->GetCurrentTime();
}

void FeaturePromoLifecycle::MaybeWriteClosedPromoData(
    FeaturePromoClosedReason close_reason) {
  if (is_demo() || wrote_close_data_) {
    return;
  }

  wrote_close_data_ = true;

  switch (close_reason) {
    case FeaturePromoClosedReason::kAction:
    case FeaturePromoClosedReason::kCancel:
    case FeaturePromoClosedReason::kDismiss:
    case FeaturePromoClosedReason::kFeatureEngaged:
    case FeaturePromoClosedReason::kTimeout: {
      ScopedPromoData data(storage_service_, iph_feature_);
      if (!app_id_.empty()) {
        data->shown_for_apps.insert(app_id_);
      }
      data->is_dismissed = true;
      data->last_dismissed_by = close_reason;
      break;
    }

    case FeaturePromoClosedReason::kSnooze: {
      ScopedPromoData data(storage_service_, iph_feature_);
      ++data->snooze_count;
      data->last_snooze_time = GetCurrentTime();
      break;
    }

    case FeaturePromoClosedReason::kAbortPromo:
    case FeaturePromoClosedReason::kOverrideForDemo:
    case FeaturePromoClosedReason::kOverrideForPrecedence:
    case FeaturePromoClosedReason::kOverrideForTesting:
    case FeaturePromoClosedReason::kOverrideForUIRegionConflict:
      // No additional action required.
      break;
  }
}

void FeaturePromoLifecycle::RecordShown() {
  // Record Promo shown
  std::string action_name = "UserEducation.MessageShown";
  base::RecordComputedAction(action_name);

  // Record Promo feature ID
  action_name.append(".");
  action_name.append(iph_feature_->name);
  base::RecordComputedAction(action_name);

  // Record Promo type
  UMA_HISTOGRAM_ENUMERATION("UserEducation.MessageShown.Type", promo_type_);
  UMA_HISTOGRAM_ENUMERATION("UserEducation.MessageShown.SubType",
                            promo_subtype_);
  std::string type_action_name = "UserEducation.MessageShown.";
  switch (promo_subtype_) {
    case PromoSubtype::kNormal:
      break;
    case PromoSubtype::kPerApp:
      // Ends with a period.
      type_action_name.append("PerApp.");
      break;
    case PromoSubtype::kLegalNotice:
      // Ends with a period.
      type_action_name.append("LegalNotice.");
      break;
  }
  switch (promo_type_) {
    case PromoType::kLegacy:
      type_action_name.append("Legacy");
      break;
    case PromoType::kToast:
      type_action_name.append("Toast");
      break;
    case PromoType::kCustomAction:
      type_action_name.append("CustomAction");
      break;
    case PromoType::kSnooze:
      type_action_name.append("Snooze");
      break;
    case PromoType::kTutorial:
      type_action_name.append("Tutorial");
      break;
    case PromoType::kUnspecified:
      NOTREACHED();
  }
  base::RecordComputedAction(type_action_name);
}

void FeaturePromoLifecycle::MaybeRecordClosedReason(
    FeaturePromoClosedReason close_reason) {
  if (is_demo() || state_ != State::kRunning) {
    return;
  }

  std::string action_name = "UserEducation.MessageAction.";

  switch (close_reason) {
    case FeaturePromoClosedReason::kDismiss:
      action_name.append("Dismiss");
      break;
    case FeaturePromoClosedReason::kSnooze:
      action_name.append("Snooze");
      break;
    case FeaturePromoClosedReason::kAction:
      action_name.append("Action");
      break;
    case FeaturePromoClosedReason::kCancel:
      action_name.append("Cancel");
      break;
    case FeaturePromoClosedReason::kTimeout:
      action_name.append("Timeout");
      break;
    case FeaturePromoClosedReason::kAbortPromo:
      action_name.append("Abort");
      break;
    case FeaturePromoClosedReason::kFeatureEngaged:
      action_name.append("FeatureEngaged");
      break;
    case FeaturePromoClosedReason::kOverrideForUIRegionConflict:
      action_name.append("OverrideForUIRegionConflict");
      break;
    case FeaturePromoClosedReason::kOverrideForPrecedence:
      action_name.append("OverrideForPrecedence");
      break;
    case FeaturePromoClosedReason::kOverrideForDemo:
      // Not used for metrics.
      return;
    case FeaturePromoClosedReason::kOverrideForTesting:
      // Not used for metrics.
      return;
  }
  action_name.append(".");
  action_name.append(iph_feature()->name);
  // Record the user action.
  base::RecordComputedAction(action_name);

  // Record the histogram.
  std::string histogram_name =
      std::string("UserEducation.MessageAction.").append(iph_feature()->name);
  base::UmaHistogramEnumeration(
      histogram_name, static_cast<FeaturePromoClosedReason>(close_reason));
}

}  // namespace user_education
