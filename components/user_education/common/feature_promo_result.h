// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_RESULT_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_RESULT_H_

#include <ostream>

#include "third_party/abseil-cpp/absl/types/optional.h"

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
  // Describes why a promo cannot currently show.
  enum Failure {
    kCanceled,          // The promo was canceled before it could show.
    kError,             // A step unexpectedly failed while showing the promo.
    kBlockedByUi,       // The promo could not be shown due to the state of the
                        // application: a conflict with other UI; the current
                        // window isn't active; the anchor isn't visible; etc.
    kBlockedByPromo,    // The promo could not be shown due to a conflict with
                        // another promotion.
    kBlockedByConfig,   // The promo could not show because it failed to meet
                        // the requirements set out in the Feature Engagement
                        // configuration.
    kSnoozed,           // The promo could not show because it is currently
                        // snoozed.
    kBlockedByContext,  // Promos are never allowed in this context (e.g. in an
                        // app that is in "off the record" or guest mode), but
                        // may be allowed in other contexts.
    kFeatureDisabled,   // The promo could not show because the `base::Feature`
                        // for the IPH is not enabled.
    kPermanentlyDismissed,  // The promo could not show because it is of a type
                            // that can be permanently dismissed, and it has
                            // been (for per-app promos, this only applies to
                            // the current app).
  };

  constexpr FeaturePromoResult() = default;
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr FeaturePromoResult(Failure reason) : failure_(reason) {}
  constexpr FeaturePromoResult(FeaturePromoResult&& other)
      : failure_(other.failure_) {}
  FeaturePromoResult(const FeaturePromoResult& other);
  ~FeaturePromoResult();
  FeaturePromoResult& operator=(const FeaturePromoResult& other);
  FeaturePromoResult& operator=(Failure reason);

  // Convenience value so a success value can be accessed the same way as the
  // various `Failure` values, e.g.:
  //   return dismissed
  //     ? FeaturePromoResult::kBlockedByUser
  //     : FeaturePromoResult::Success();
  static FeaturePromoResult Success();

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator bool() const { return !failure_.has_value(); }
  constexpr bool operator!() const { return failure_.has_value(); }
  constexpr bool operator==(const FeaturePromoResult& other) const {
    return failure_ == other.failure_;
  }
  constexpr bool operator!=(const FeaturePromoResult& other) const {
    return failure_ != other.failure_;
  }
  constexpr auto failure() const { return failure_; }

 private:
  absl::optional<Failure> failure_;
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
