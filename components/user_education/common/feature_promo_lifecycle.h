// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_LIFECYCLE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_LIFECYCLE_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble.h"

namespace base {
struct Feature;
}

namespace user_education {

// Implements business logic around the lifecycle of an IPH feature promo,
// depending on promo type and subtype. Tracks information about the IPH in
// question through `FeaturePromoStorageService`.
class FeaturePromoLifecycle {
 public:
  using PromoSubtype = FeaturePromoSpecification::PromoSubtype;
  using PromoType = FeaturePromoSpecification::PromoType;

  FeaturePromoLifecycle(FeaturePromoStorageService* storage_service,
                        std::string_view promo_key,
                        const base::Feature* iph_feature,
                        PromoType promo_type,
                        PromoSubtype promo_subtype,
                        int num_rotating_entries);
  ~FeaturePromoLifecycle();

  FeaturePromoLifecycle(const FeaturePromoLifecycle&) = delete;
  void operator=(const FeaturePromoLifecycle&) = delete;

  // Sets reshow policy, if there is one.
  FeaturePromoLifecycle& SetReshowPolicy(base::TimeDelta reshow_time,
                                         std::optional<int> max_show_count);

  // Returns whether the policy and previous usage of this IPH would allow it to
  // be shown again; for example, a snoozeable IPH cannot show if it is
  // currently in the snooze period.
  //
  // For certain types of high-priority promos, `reshow_time` and
  // `max_reshow_count` may be used to allow re-showing previously-dismissed
  // promos.
  FeaturePromoResult CanShow() const;

  // Returns whether the policy and previous usage of this IPH would allow it to
  // be snoozed if it were shown; meaningless if `CanShow()` is false.
  bool CanSnooze() const;

  // For rotating promos only; retrieves the current promo index. The index will
  // be incremented when the promo data is written. The value returned will be
  // clamped to the number of rotating promo entries, exclusive, such that if
  // the current value exceeds the number of entries it will wrap around to 0.
  int GetPromoIndex() const;

  // Allows the current promo index to be set (rotating promos only). This is
  // useful if a previously-available promo has been removed, leaving a gap in
  // the rotation at the current index.
  //
  // For example, say the promos are {P1, null, P2, P3}. Then if GetPromoIndex()
  // returns 1, the calling code may see the null value and increment the index
  // to 2. Setting it back on the lifecycle ensures that the correct next index
  // is stored on completion, so that P3 is shown next time rather than showing
  // P2 again.
  void SetPromoIndex(int new_index);

  // Notifies that the promo was shown. `tracker` will be used to release the
  // feature when the promo ends.
  void OnPromoShown(std::unique_ptr<HelpBubble> help_bubble,
                    feature_engagement::Tracker* tracker);

  // Notifies that the promo is being shown for a demo. No data will be stored.
  void OnPromoShownForDemo(std::unique_ptr<HelpBubble> help_bubble);

  // Notifies that the promo bubble closed. If none of the other dismissed,
  // snoozed, continued, etc. callbacks have been called, assumes that the
  // bubble was closed programmatically without user input.
  //
  // Returns whether the promo was aborted as a result.
  bool OnPromoBubbleClosed(HelpBubble::CloseReason close_reason);

  // Notifies that the promo was ended for the specified `close_reason`.
  // May result in pref data and/or histogram logging. If `continue_promo` is
  // true, the bubble should be closed but the promo is not ended -
  // `OnContinuedPromoEnded()` should be called when the continuation finishes.
  void OnPromoEnded(FeaturePromoClosedReason close_reason,
                    bool continue_promo = false);

  // For custom action, Tutorial, etc. indicates that the
  void OnContinuedPromoEnded(bool completed_successfully);

  HelpBubble* help_bubble() { return help_bubble_.get(); }
  const HelpBubble* help_bubble() const { return help_bubble_.get(); }
  const base::Feature* iph_feature() const { return iph_feature_; }
  bool was_started() const { return state_ != State::kNotStarted; }
  bool is_promo_active() const {
    return state_ == State::kRunning || state_ == State::kContinued;
  }
  bool is_bubble_visible() const { return state_ == State::kRunning; }
  bool is_demo() const { return was_started() && !tracker_; }
  PromoType promo_type() const { return promo_type_; }
  PromoSubtype promo_subtype() const { return promo_subtype_; }

 private:
  enum class State { kNotStarted, kRunning, kContinued, kClosed };

  // Records `PromoData` about the promo closing, unless in demo mode.
  void MaybeWriteClosedPromoData(FeaturePromoClosedReason close_reason);

  // Records that an IPH was shown, including type and identity.
  void RecordShown();

  // Records user actions and histograms that discern what action was taken to
  // close a promotion. Does not record in demo mode.
  void MaybeRecordClosedReason(FeaturePromoClosedReason close_reason);

  // If the promo is running, ends it, possibly dismissing the Tracker.
  bool MaybeEndPromo();

  // Returns whether `promo_data` satisfies the requirements for being shown as
  // a snooze promo.
  FeaturePromoResult CanShowSnoozePromo(
      const FeaturePromoData& promo_data) const;

  // Gets the current time, which is based on the reference clock provided by
  // `storage_service` and can be overridden by tests.
  base::Time GetCurrentTime() const;

  // If `promo_index_` isn't yet known, reads it from `data`; if `data` is null,
  // reads it from the storage service instead.
  void MaybeCachePromoIndex(const FeaturePromoData* data) const;

  // Returns the result of trying to reshow a promo.
  FeaturePromoResult GetReshowResult(base::Time last_show_time,
                                     int show_count) const;

  // The service that stores non-transient data about the IPH.
  const raw_ptr<FeaturePromoStorageService> storage_service_;

  // The current promo key, or empty for none.
  const std::string promo_key_;

  // Data about the current IPH.
  const raw_ptr<const base::Feature> iph_feature_;
  const PromoType promo_type_;
  const PromoSubtype promo_subtype_;
  const int num_rotating_entries_;

  // Optional data about the current IPH.
  std::optional<base::TimeDelta> reshow_delay_;
  std::optional<int> max_show_count_;

  // These are cached values for rotating promos that are applied later when the
  // data for the promo is updated.
  mutable std::optional<int> promo_index_;

  // The current state of the promo.
  State state_ = State::kNotStarted;
  bool wrote_close_data_ = false;
  bool tracker_dismissed_ = false;

  // The tracker, in the event the bubble needs to be dismissed.
  raw_ptr<feature_engagement::Tracker> tracker_ = nullptr;
  std::unique_ptr<HelpBubble> help_bubble_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_LIFECYCLE_H_
