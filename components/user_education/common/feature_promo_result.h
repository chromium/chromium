// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_RESULT_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_RESULT_H_

#include <optional>
#include <ostream>

namespace user_education {

// Type that expresses the result of trying to show a feature promo as either
// `kSuccess` or one of the given `Failure` reasons. Also used as the return
// value of methods which check whether a promo could show.
//
// This object has a "truthy" value (kSuccess) and a list of "falsy" values
// (any of the values of `Failure`). It can therefore be treated as both a
// boolean and (if false) an error code.
class FeaturePromoResult {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Describes why a promo cannot currently show.
  enum Failure {
    kCanceled = 0,     // The promo was canceled before it could show.
    kError = 1,        // A step unexpectedly failed while showing the promo.
    kBlockedByUi = 2,  // The promo could not be shown due to the state of the
                       // application: a conflict with other UI; the current
                       // window isn't active; the anchor isn't visible; etc.
    kBlockedByPromo = 3,  // The promo could not be shown due to a conflict with
                          // another promotion.
    kBlockedByConfig =
        4,  // The promo could not show because it failed to meet the
            // requirements set out in the Feature Engagement configuration.
    kSnoozed = 5,  // The promo could not show because it is currently snoozed.
    kBlockedByContext = 6,  // Promos are never allowed in this context (e.g. in
                            // an app that is in "off the record" or guest
                            // mode), but may be allowed in other contexts.
    kFeatureDisabled = 7,   // The promo could not show because the
                            // `base::Feature` for the IPH is not enabled.
    kPermanentlyDismissed =
        8,  // The promo could not show because it is of a type that can be
            // permanently dismissed, and it has been (for per-app promos, this
            // only applies to the current app).
    kBlockedByGracePeriod = 9,  // The promo could not be shown due to the
                                // session startup grace period.
    kBlockedByCooldown =
        10,  // The promo could not be shown because it hasn't been long enough
             // since the last heavyweight promo.
    kRecentlyAborted = 11,  // The promo recently aborted due to a UI change and
                            // cannot be shown again for a short period of time.
    kExceededMaxShowCount = 12,  // The promo has been shown so many times that
                                 // it should be considered permanently
                                 // dismissed.
    kBlockedByNewProfile = 13,  // The promo could not be shown because the user
                                // is still inside the new profile grace period.
    kBlockedByReshowDelay = 14,  // The promo is allowed to reshow after
                                 // dismissal, but the required time has not
                                 // elapsed yet.
    kMaxValue = kBlockedByReshowDelay
  };

  constexpr FeaturePromoResult() = default;
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr FeaturePromoResult(Failure reason) : failure_(reason) {}
  constexpr FeaturePromoResult(FeaturePromoResult&& other) noexcept
      : failure_(other.failure_) {}
  FeaturePromoResult(const FeaturePromoResult& other);
  FeaturePromoResult& operator=(const FeaturePromoResult& other);
  FeaturePromoResult& operator=(Failure reason);
  ~FeaturePromoResult();

  // Convenience value so a success value can be accessed the same way as the
  // various `Failure` values, e.g.:
  //   return dismissed
  //     ? FeaturePromoResult::kBlockedByUser
  //     : FeaturePromoResult::Success();
  static FeaturePromoResult Success();

  // Returns true if the promo can never show again.
  constexpr bool is_permanently_blocked() const {
    return failure_ == kPermanentlyDismissed ||
           failure_ == kExceededMaxShowCount;
  }

  // Returns true if the promo is unlikely to show in this browser instance.
  constexpr bool is_blocked_this_instance() const {
    return is_permanently_blocked() || failure_ == kFeatureDisabled ||
           failure_ == kBlockedByContext;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator bool() const { return !failure_.has_value(); }
  constexpr bool operator!() const { return failure_.has_value(); }
  constexpr bool operator==(const FeaturePromoResult& other) const {
    return failure_ == other.failure_;
  }
  constexpr bool operator!=(const FeaturePromoResult& other) const {
    return failure_ != other.failure_;
  }
  constexpr bool operator==(Failure other) const { return failure_ == other; }
  constexpr bool operator!=(Failure other) const { return failure_ != other; }
  constexpr auto failure() const { return failure_; }

 private:
  std::optional<Failure> failure_;
};

constexpr bool operator==(FeaturePromoResult::Failure f,
                          const FeaturePromoResult& other) {
  return !other && f == other.failure();
}
constexpr bool operator!=(FeaturePromoResult::Failure f,
                          const FeaturePromoResult& other) {
  return !other && f != other.failure();
}

std::ostream& operator<<(std::ostream& os, FeaturePromoResult::Failure failure);
std::ostream& operator<<(std::ostream& os, const FeaturePromoResult& result);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_RESULT_H_
