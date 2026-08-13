// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_
#define COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/variations/service/variations_service.h"

class PrefService;
class ScopedDictPrefUpdate;

namespace universal_optout {

// Service responsible for tracking location history and determining eligibility
// of users for Universal Opt Out.
class UniversalOptOutService : public KeyedService,
                               public variations::VariationsService::Observer {
 public:
  static constexpr double kEligibilityThresholdRatio = 0.5;

  explicit UniversalOptOutService(
      PrefService& pref_service,
      variations::VariationsService& variations_service,
      const base::Clock& clock = *base::DefaultClock::GetInstance());

  UniversalOptOutService(const UniversalOptOutService&) = delete;
  UniversalOptOutService& operator=(const UniversalOptOutService&) = delete;

  ~UniversalOptOutService() override;

  // KeyedService:
  void Shutdown() override;

  // variations::VariationsService::Observer:
  void OnSeedFetched() override;

  // Returns whether the profile is eligible for Universal Opt Out.
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

  // Returns the start of the current day (UTC midnight) for now.
  base::Time GetCurrentDay() const;

  raw_ref<PrefService> pref_service_;
  raw_ref<variations::VariationsService> variations_service_;
  raw_ref<const base::Clock> clock_;

  base::ScopedObservation<variations::VariationsService,
                          variations::VariationsService::Observer>
      variations_service_observation_{this};
};

}  // namespace universal_optout

#endif  // COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_
