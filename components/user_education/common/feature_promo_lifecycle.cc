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
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble.h"

namespace user_education {

namespace {

FeaturePromoResult CanShowSnoozePromo(
    const FeaturePromoStorageService::PromoData& promo_data) {
  // This IPH has been dismissed by user permanently.
  if (promo_data.is_dismissed) {
    return FeaturePromoResult::kPermanentlyDismissed;
  }

  // This IPH is shown for the first time.
  if (promo_data.show_count == 0) {
    return FeaturePromoResult::Success();
  }

  // Corruption: Snooze time is in the future.
  if (promo_data.last_snooze_time > base::Time::Now()) {
    return FeaturePromoResult::kSnoozed;
  }

  // Use the snooze duration if this promo was snoozed the last time it was
  // shown; otherwise use the default duration.
  // TODO(dfried): Revisit this for 2.0.
  const base::TimeDelta snooze_time =
      (promo_data.last_snooze_time > promo_data.last_show_time)
          ? promo_data.last_snooze_duration
          : FeaturePromoLifecycle::kDefaultSnoozeDuration;

  // The IPH was snoozed, so it shouldn't be shown again until the snooze
  // duration ends.
  return base::Time::Now() >= (promo_data.last_show_time + snooze_time)
             ? FeaturePromoResult::Success()
             : FeaturePromoResult::kSnoozed;
}

class ScopedPromoData {
 public:
  ScopedPromoData(FeaturePromoStorageService* storage_service,
                  const base::Feature* iph_feature)
      : storage_service_(storage_service), iph_feature_(iph_feature) {
    promo_data_ = storage_service_->ReadPromoData(*iph_feature)
                      .value_or(FeaturePromoStorageService::PromoData());
  }

  ~ScopedPromoData() {
    storage_service_->SavePromoData(*iph_feature_, promo_data_);
  }

  FeaturePromoStorageService::PromoData* operator->() { return &promo_data_; }

 private:
  FeaturePromoStorageService::PromoData promo_data_;
  const raw_ptr<FeaturePromoStorageService> storage_service_;
  const raw_ptr<const base::Feature> iph_feature_;
};

}  // namespace

constexpr base::TimeDelta FeaturePromoLifecycle::kDefaultSnoozeDuration;

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
  ++data->show_count;
  data->last_show_time = base::Time::Now();
  RecordShown();
}

void FeaturePromoLifecycle::OnPromoShownForDemo(
    std::unique_ptr<HelpBubble> help_bubble) {
  OnPromoShown(std::move(help_bubble), nullptr);
}

bool FeaturePromoLifecycle::OnPromoBubbleClosed() {
  help_bubble_.reset();
  if (state_ == State::kRunning) {
    MaybeRecordCloseReason(CloseReason::kAbortPromo);
    CHECK(MaybeEndPromo());
    return true;
  }
  return false;
}

void FeaturePromoLifecycle::OnPromoEnded(CloseReason close_reason,
                                         bool continue_promo) {
  MaybeRecordCloseReason(close_reason);
  if (continue_promo) {
    CHECK(is_bubble_visible());
    state_ = State::kContinued;
    // When a snoozeable, normal promo that has a follow-up action (Tutorial,
    // custom action), the result is not recorded until after the follow-up
    // finishes, because e.g. an aborted tutorial counts as a snooze.
    if (promo_subtype_ != PromoSubtype::kNormal ||
        close_reason != CloseReason::kAction) {
      MaybeWriteClosePromoData(close_reason);
    }
    help_bubble_->Close();
  } else {
    CHECK(MaybeEndPromo());
    MaybeWriteClosePromoData(close_reason);
  }
}

void FeaturePromoLifecycle::OnContinuedPromoEnded(bool completed_successfully) {
  MaybeWriteClosePromoData(completed_successfully ? CloseReason::kAction
                                                  : CloseReason::kSnooze);
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

void FeaturePromoLifecycle::MaybeWriteClosePromoData(CloseReason close_reason) {
  if (is_demo() || wrote_close_data_) {
    return;
  }

  wrote_close_data_ = true;

  switch (close_reason) {
    case CloseReason::kAction:
    case CloseReason::kCancel:
    case CloseReason::kDismiss:
    case CloseReason::kFeatureEngaged:
    case CloseReason::kTimeout: {
      ScopedPromoData data(storage_service_, iph_feature_);
      if (!app_id_.empty()) {
        data->shown_for_apps.insert(app_id_);
      }
      data->is_dismissed = true;
      data->last_dismissed_by = close_reason;
      break;
    }

    case CloseReason::kSnooze: {
      ScopedPromoData data(storage_service_, iph_feature_);
      ++data->snooze_count;
      data->last_snooze_time = base::Time::Now();
      data->last_snooze_duration = kDefaultSnoozeDuration;
      break;
    }

    case CloseReason::kAbortPromo:
    case CloseReason::kOverrideForDemo:
    case CloseReason::kOverrideForPrecedence:
    case CloseReason::kOverrideForTesting:
    case CloseReason::kOverrideForUIRegionConflict:
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

void FeaturePromoLifecycle::MaybeRecordCloseReason(CloseReason close_reason) {
  if (is_demo() || state_ != State::kRunning) {
    return;
  }

  std::string action_name = "UserEducation.MessageAction.";

  switch (close_reason) {
    case CloseReason::kDismiss:
      action_name.append("Dismiss");
      break;
    case CloseReason::kSnooze:
      action_name.append("Snooze");
      break;
    case CloseReason::kAction:
      action_name.append("Action");
      break;
    case CloseReason::kCancel:
      action_name.append("Cancel");
      break;
    case CloseReason::kTimeout:
      action_name.append("Timeout");
      break;
    case CloseReason::kAbortPromo:
      action_name.append("Abort");
      break;
    case CloseReason::kFeatureEngaged:
      action_name.append("FeatureEngaged");
      break;
    case CloseReason::kOverrideForUIRegionConflict:
      action_name.append("OverrideForUIRegionConflict");
      break;
    case CloseReason::kOverrideForPrecedence:
      action_name.append("OverrideForPrecedence");
      break;
    case CloseReason::kOverrideForDemo:
      // Not used for metrics.
      return;
    case CloseReason::kOverrideForTesting:
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
  base::UmaHistogramEnumeration(histogram_name,
                                static_cast<CloseReason>(close_reason));
}

}  // namespace user_education
