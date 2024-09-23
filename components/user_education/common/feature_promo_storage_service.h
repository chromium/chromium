// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_

#include <map>
#include <optional>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"

// Declare in the global namespace for test purposes.
class FeaturePromoStorageInteractiveTest;
class UserEducationInternalsPageHandlerImpl;

namespace user_education {

// This service manages snooze and other display data for in-product help
// promos.
//
// It is an abstract base class in order to support multiple frameworks/
// platforms, and different data stores.
//
// Before showing an IPH, the IPH controller should ask if the IPH is blocked.
// The controller should also notify after the IPH is shown and after the user
// clicks the snooze/dismiss button.
class FeaturePromoStorageService {
 public:
  FeaturePromoStorageService();
  virtual ~FeaturePromoStorageService();

  // Disallow copy and assign.
  FeaturePromoStorageService(const FeaturePromoStorageService&) = delete;
  FeaturePromoStorageService& operator=(const FeaturePromoStorageService&) =
      delete;

  virtual std::optional<FeaturePromoData> ReadPromoData(
      const base::Feature& iph_feature) const = 0;

  virtual void SavePromoData(const base::Feature& iph_feature,
                             const FeaturePromoData& promo_data) = 0;

  // Reset the state of |iph_feature|.
  virtual void Reset(const base::Feature& iph_feature) = 0;

  virtual FeaturePromoSessionData ReadSessionData() const = 0;

  virtual void SaveSessionData(const FeaturePromoSessionData& session_data) = 0;

  virtual void ResetSession() = 0;

  virtual FeaturePromoPolicyData ReadPolicyData() const = 0;

  virtual void SavePolicyData(const FeaturePromoPolicyData& policy_data) = 0;

  // Reset the policy data.
  virtual void ResetPolicy() = 0;

  virtual user_education::NewBadgeData ReadNewBadgeData(
      const base::Feature& new_badge_feature) const = 0;

  virtual void SaveNewBadgeData(const base::Feature& new_badge_feature,
                                const NewBadgeData& new_badge_data) = 0;

  // Resets the state of `new_badge_feature`.
  virtual void ResetNewBadge(const base::Feature& new_badge_feature) = 0;

  // Returns the set of apps that `iph_feature` has been shown for.
  KeyedFeaturePromoDataMap GetKeyedPromoData(
      const base::Feature& iph_feature) const;

  // Returns the count of previous snoozes for `iph_feature`.
  int GetSnoozeCount(const base::Feature& iph_feature) const;

  // Returns the current time, as per `clock_`, which defaults to
  // `base::DefaultClock`.
  virtual base::Time GetCurrentTime() const;

  // Gets the time when the current profile was created.
  base::Time profile_creation_time() const { return profile_creation_time_; }

  void set_profile_creation_time_for_testing(base::Time profile_creation_time) {
    set_profile_creation_time(profile_creation_time);
  }

  // Sets the clock used across user education for session logic.
  void set_clock_for_testing(const base::Clock* clock) { clock_ = clock; }

 protected:
  friend UserEducationInternalsPageHandlerImpl;

  // Sets the profile creation time; used by derived classes.
  void set_profile_creation_time(base::Time profile_creation_time) {
    profile_creation_time_ = profile_creation_time;
  }

 private:
  // Time the current user's profile was created on this device; used to
  // determine if low-priority promos should show (there is a grace period after
  // new profile creation).
  base::Time profile_creation_time_;

  raw_ptr<const base::Clock> clock_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_
