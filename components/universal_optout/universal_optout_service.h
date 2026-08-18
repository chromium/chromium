// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_
#define COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/variations/service/variations_service.h"

class PrefService;
class ScopedDictPrefUpdate;

namespace signin {
class IdentityManager;
enum class Tribool;
}  // namespace signin

namespace universal_optout {

// Histogram names.
inline constexpr char kProfileEligibilityStartupHistogram[] =
    "Privacy.UniversalOptOut.ProfileEligibility.Startup";
inline constexpr char kEligibilitySystemStartupHistogram[] =
    "Privacy.UniversalOptOut.EligibilitySystem.Startup";
inline constexpr char kEligibilityChangedHistogram[] =
    "Privacy.UniversalOptOut.EligibilityChanged";

// Identifies which system determined eligibility and the result.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(UniversalOptOutEligibilitySystem)
enum class EligibilitySystem {
  kEligibleViaAccountCapabilities = 0,
  kIneligibleViaAccountCapabilities = 1,
  kEligibleViaFinch = 2,
  kIneligibleViaFinch = 3,
  kMaxValue = kIneligibleViaFinch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:UniversalOptOutEligibilitySystem)

// Indicates the transition direction when eligibility changes.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(UniversalOptOutEligibilityTransition)
enum class EligibilityTransition {
  kIneligibleToEligible = 0,
  kEligibleToIneligible = 1,
  kMaxValue = kEligibleToIneligible,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:UniversalOptOutEligibilityTransition)

// Service responsible for tracking location history and determining eligibility
// of users for Universal Opt Out. For signed-out users, eligibility is
// determined by tracking location history over a sliding window. For signed-in
// users, eligibility is determined by the user's account capabilities.
class UniversalOptOutService : public KeyedService,
                               public variations::VariationsService::Observer {
 public:
  static constexpr double kEligibilityThresholdRatio = 0.5;

  explicit UniversalOptOutService(
      PrefService& pref_service,
      variations::VariationsService& variations_service,
      signin::IdentityManager& identity_manager,
      const base::Clock& clock = *base::DefaultClock::GetInstance());

  UniversalOptOutService(const UniversalOptOutService&) = delete;
  UniversalOptOutService& operator=(const UniversalOptOutService&) = delete;

  ~UniversalOptOutService() override;

  // KeyedService:
  void Shutdown() override;

  // variations::VariationsService::Observer:
  void OnSeedFetched() override;
  void OnVariationsServiceDestroyed() override;

  // Returns whether the profile is eligible for Universal Opt Out.
  // For signed-in users, this uses the AccountCapabilities signal (falling back
  // to location history if the capability is unknown).
  // For signed-out users, this is based on recorded location history.
  bool IsEligible() const;

 private:
  // Records the current location for today if not already recorded, prunes
  // history older than the retention window, and updates eligibility.
  void RecordLocationAndUpdateEligibility();

  // Records location for `current_day` in `kUniversalOptOutEligibilityHistory`.
  void RecordLocation(base::Time current_day,
                      ScopedDictPrefUpdate& history_update);

  // Prunes history records older than the retention window from `current_day`
  // in `kUniversalOptOutEligibilityHistory`.
  void PruneExpiredHistory(base::Time current_day,
                           ScopedDictPrefUpdate& history_update);

  // Evaluates and updates eligibility status based on location history within
  // the sliding windows.
  void UpdateEligibility(base::Time current_day);

  // Returns the eligibility status based on account capabilities if known,
  // or Tribool::kUnknown if unknown or signed out.
  signin::Tribool GetAccountCapabilityEligibility() const;

  // Records startup metrics (profile eligibility and eligibility system).
  void RecordStartupMetrics();

  // Returns the start of the current day (UTC midnight) for now.
  base::Time GetCurrentDay() const;

  raw_ref<PrefService> pref_service_;
  raw_ptr<variations::VariationsService> variations_service_;
  raw_ref<signin::IdentityManager> identity_manager_;
  raw_ref<const base::Clock> clock_;

  base::ScopedObservation<variations::VariationsService,
                          variations::VariationsService::Observer>
      variations_service_observation_{this};
};

}  // namespace universal_optout

#endif  // COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_
