// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_lifecycle.h"

#include <optional>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
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

  FeaturePromoData* get() { return &promo_data_; }
  FeaturePromoData* operator->() { return &promo_data_; }

 private:
  FeaturePromoData promo_data_;
  const raw_ptr<FeaturePromoStorageService> storage_service_;
  const raw_ptr<const base::Feature> iph_feature_;
};

}  // namespace

FeaturePromoLifecycle::FeaturePromoLifecycle(
    FeaturePromoStorageService* storage_service,
    std::string_view promo_key,
    const base::Feature* iph_feature,
    PromoType promo_type,
    PromoSubtype promo_subtype,
    int num_rotating_entries = 0)
    : storage_service_(storage_service),
      promo_key_(promo_key),
      iph_feature_(iph_feature),
      promo_type_(promo_type),
      promo_subtype_(promo_subtype),
      num_rotating_entries_(num_rotating_entries) {
  CHECK_EQ(promo_type_ == PromoType::kRotating, num_rotating_entries_ > 0)
      << "Number of rotating entries should be nonzero if and only if promo "
         "type is rotating.";
}

FeaturePromoLifecycle::~FeaturePromoLifecycle() {
  MaybeEndPromo();
}

FeaturePromoLifecycle& FeaturePromoLifecycle::SetReshowPolicy(
    base::TimeDelta reshow_delay,
    std::optional<int> max_show_count) {
  CHECK_NE(promo_subtype_, PromoSubtype::kNormal)
      << "Reshow not allowed for normal promos.";
  reshow_delay_ = reshow_delay;
  max_show_count_ = max_show_count;
  return *this;
}

FeaturePromoResult FeaturePromoLifecycle::CanShow() const {
  CHECK(promo_subtype_ != PromoSubtype::kKeyedNotice || !promo_key_.empty());

  // If inside the new profile grace period, the promo cannot show and would
  // also not have shown yet.
  if (promo_subtype_ == PromoSubtype::kNormal &&
      GetCurrentTime() < storage_service_->profile_creation_time() +
                             features::GetNewProfileGracePeriod()) {
    return FeaturePromoResult::kBlockedByNewProfile;
  }

  const auto data = storage_service_->ReadPromoData(*iph_feature_);
  if (!data.has_value()) {
    return FeaturePromoResult::Success();
  }

  switch (promo_subtype_) {
    case PromoSubtype::kNormal: {
      // Check dismissed/snoozed state.
      FeaturePromoResult result;
      switch (promo_type_) {
        case PromoType::kLegacy:
        case PromoType::kToast:
          result = data->is_dismissed
                       ? FeaturePromoResult::kPermanentlyDismissed
                       : FeaturePromoResult::Success();
          break;
        case PromoType::kCustomAction:
        case PromoType::kSnooze:
        case PromoType::kTutorial:
          result = CanShowSnoozePromo(*data);
          break;
        case PromoType::kRotating:
          // Cache the next promo index for later.
          MaybeCachePromoIndex(&*data);
          // For now, rotating promos can continue to show indefinitely.
          return FeaturePromoResult::Success();
        case PromoType::kUnspecified:
          NOTREACHED_IN_MIGRATION();
          result = FeaturePromoResult::kPermanentlyDismissed;
          break;
      }

      // Even if the promo could show, it may have exceeded its maximum show
      // count. This is always the last consideration since it is the least
      // descriptive return value.
      //
      // Note that snoozes do not count towards the show cap; the cap only
      // applies to showing when the IPH is neither dismissed nor snoozed.
      if (result && features::IsUserEducationV2() &&
          data->show_count - data->snooze_count >=
              features::GetMaxPromoShowCount()) {
        result = FeaturePromoResult::kExceededMaxShowCount;
      }
      return result;
    }
    case PromoSubtype::kKeyedNotice:
      if (const auto* const key_data =
              base::FindOrNull(data->shown_for_keys, promo_key_)) {
        return GetReshowResult(key_data->last_shown_time, key_data->show_count);
      }
      return FeaturePromoResult::Success();
    case PromoSubtype::kLegalNotice:
    case PromoSubtype::kActionableAlert:
      if (data->is_dismissed) {
        return GetReshowResult(data->last_show_time,
                               data->show_count - data->snooze_count);
      }
      return FeaturePromoResult::Success();
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
    case PromoType::kRotating:
      // TODO(dfried): Should snooze promos be allowed in rotating promos?
      return true;
    case PromoType::kUnspecified:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

int FeaturePromoLifecycle::GetPromoIndex() const {
  CHECK_EQ(PromoType::kRotating, promo_type_);
  MaybeCachePromoIndex(nullptr);
  return *promo_index_;
}

void FeaturePromoLifecycle::SetPromoIndex(int new_index) {
  CHECK_EQ(PromoType::kRotating, promo_type_);
  CHECK_LT(new_index, num_rotating_entries_);
  promo_index_ = new_index;
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

bool FeaturePromoLifecycle::OnPromoBubbleClosed(
    HelpBubble::CloseReason reason) {
  help_bubble_.reset();
  if (state_ == State::kRunning) {
    FeaturePromoClosedReason closed_reason;
    switch (reason) {
      case HelpBubble::CloseReason::kProgrammaticallyClosed:
        // A different close reason should have already been recorded elsewhere.
        closed_reason = FeaturePromoClosedReason::kAbortPromo;
        break;
      case HelpBubble::CloseReason::kAnchorHidden:
        closed_reason = FeaturePromoClosedReason::kAbortedByAnchorHidden;
        break;
      case HelpBubble::CloseReason::kBubbleElementDestroyed:
      case HelpBubble::CloseReason::kBubbleDestroyed:
        closed_reason = FeaturePromoClosedReason::kAbortedByBubbleDestroyed;
        break;
    }
    MaybeRecordClosedReason(closed_reason);
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
    help_bubble_->Close(HelpBubble::CloseReason::kProgrammaticallyClosed);
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
    const auto snooze_time = features::GetSnoozeDuration();
    if (now < promo_data.last_snooze_time + snooze_time) {
      return FeaturePromoResult::kSnoozed;
    }
    if (now < promo_data.last_show_time + snooze_time) {
      return FeaturePromoResult::kRecentlyAborted;
    }
  }

  return FeaturePromoResult::Success();
}

base::Time FeaturePromoLifecycle::GetCurrentTime() const {
  return storage_service_->GetCurrentTime();
}

void FeaturePromoLifecycle::MaybeCachePromoIndex(
    const FeaturePromoData* data) const {
  if (promo_index_.has_value()) {
    return;
  }
  if (data) {
    promo_index_ = data->promo_index;
  } else {
    const auto new_data = storage_service_->ReadPromoData(*iph_feature_);
    if (!new_data) {
      promo_index_ = 0;
      return;
    }
    promo_index_ = new_data->promo_index;
  }

  // If promo index was just read, it may exceed the number of entries; if this
  // is the case, wrap back around to zero.
  if (promo_index_ >= num_rotating_entries_) {
    promo_index_ = 0;
  }
}

FeaturePromoResult FeaturePromoLifecycle::GetReshowResult(
    base::Time last_shown,
    int show_count) const {
  if (!reshow_delay_) {
    return FeaturePromoResult::kPermanentlyDismissed;
  }
  if (max_show_count_ && show_count >= *max_show_count_) {
    return FeaturePromoResult::kPermanentlyDismissed;
  }
  if (GetCurrentTime() - last_shown < *reshow_delay_) {
    return FeaturePromoResult::kBlockedByReshowDelay;
  }
  return FeaturePromoResult::Success();
}

void FeaturePromoLifecycle::MaybeWriteClosedPromoData(
    FeaturePromoClosedReason close_reason) {
  if (wrote_close_data_) {
    return;
  }

  wrote_close_data_ = true;

  const bool is_rotating = promo_type_ == PromoType::kRotating;

  if (is_demo()) {
    // Increment promo index for rotating promos even in demo mode so that each
    // promo in the rotation can be tested.
    if (is_rotating) {
      ScopedPromoData data(storage_service_, iph_feature_);
      MaybeCachePromoIndex(data.get());
      data->promo_index = GetPromoIndex() + 1;
    }
    // Don't write any other data for demo promos.
    return;
  }

  switch (close_reason) {
    case FeaturePromoClosedReason::kAction:
    case FeaturePromoClosedReason::kCancel:
    case FeaturePromoClosedReason::kDismiss:
    case FeaturePromoClosedReason::kFeatureEngaged:
    case FeaturePromoClosedReason::kTimeout: {
      ScopedPromoData data(storage_service_, iph_feature_);
      if (!promo_key_.empty()) {
        auto& key_data = data->shown_for_keys[promo_key_];
        ++key_data.show_count;
        key_data.last_shown_time = GetCurrentTime();
      }
      data->last_dismissed_by = close_reason;
      if (is_rotating) {
        MaybeCachePromoIndex(data.get());
        data->promo_index = GetPromoIndex() + 1;
      } else {
        data->is_dismissed = true;
      }
      break;
    }

    case FeaturePromoClosedReason::kSnooze: {
      ScopedPromoData data(storage_service_, iph_feature_);
      ++data->snooze_count;
      data->last_snooze_time = GetCurrentTime();
      // TODO(dfried): Should rotating promos that have been snoozed increment?
      // Should snooze promos even be allowed in a rotating promo?
      break;
    }

    case FeaturePromoClosedReason::kAbortPromo:
    case FeaturePromoClosedReason::kOverrideForDemo:
    case FeaturePromoClosedReason::kOverrideForPrecedence:
    case FeaturePromoClosedReason::kOverrideForTesting:
    case FeaturePromoClosedReason::kOverrideForUIRegionConflict:
    case FeaturePromoClosedReason::kAbortedByFeature:
    case FeaturePromoClosedReason::kAbortedByAnchorHidden:
    case FeaturePromoClosedReason::kAbortedByBubbleDestroyed:
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

  std::string type_action_name = "UserEducation.MessageShown.";
  switch (promo_subtype_) {
    case PromoSubtype::kNormal:
      break;
    case PromoSubtype::kKeyedNotice:
      // Ends with a period.
      type_action_name.append("KeyedNotice.");
      break;
    case PromoSubtype::kLegalNotice:
      // Ends with a period.
      type_action_name.append("LegalNotice.");
      break;
    case PromoSubtype::kActionableAlert:
      // Ends with a period.
      type_action_name.append("ActionableAlert.");
      break;
  }

  UMA_HISTOGRAM_ENUMERATION("UserEducation.MessageShown.Subtype",
                            promo_subtype_);

  UMA_HISTOGRAM_ENUMERATION("UserEducation.MessageShown.Type", promo_type_);
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
    case PromoType::kRotating: {
      // Want to track exactly which messages are being shown in a rotating
      // promo. Because the suggested cap on exact linear histograms is 100, use
      // that as the max allowed value.
      //
      // Note that the +1 is not an increment, but rather reflects that
      // histograms are 1-indexed rather than 0-indexed. Therefore, the first
      // promo is promo 1, not 0.
      const std::string histogram_name =
          std::string("UserEducation.RotatingPromoIndex.")
              .append(iph_feature()->name);
      base::UmaHistogramExactLinear(histogram_name, GetPromoIndex() + 1, 100);
      type_action_name.append("Rotating");
      break;
    }
    case PromoType::kUnspecified:
      NOTREACHED_IN_MIGRATION();
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
    case FeaturePromoClosedReason::kAbortedByFeature:
      action_name.append("AbortedByFeature");
      break;
    case FeaturePromoClosedReason::kAbortedByAnchorHidden:
      action_name.append("AbortedByAnchorHidden");
      break;
    case FeaturePromoClosedReason::kAbortedByBubbleDestroyed:
      action_name.append("AbortedByBubbleDestroyed");
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
  const std::string histogram_name =
      std::string("UserEducation.MessageAction.").append(iph_feature()->name);
  base::UmaHistogramEnumeration(
      histogram_name, static_cast<FeaturePromoClosedReason>(close_reason));
}

}  // namespace user_education
