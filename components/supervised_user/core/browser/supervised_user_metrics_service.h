// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_METRICS_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_METRICS_SERVICE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "supervised_user_service.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Time;
}  // namespace base

namespace supervised_user {
class SupervisedUserURLFilter;

// Service to initialize and control metric recorders of supervised users.
// Records metrics daily, or when the SupervisedUserService changes.
class SupervisedUserMetricsService : public KeyedService,
                                     public SupervisedUserServiceObserver {
 public:
  // Delegate for recording metrics relating to extensions for supervised users
  // such as metrics that should be recorded daily.
  class SupervisedUserMetricsServiceExtensionDelegate {
   public:
    virtual ~SupervisedUserMetricsServiceExtensionDelegate() = default;
    // Record metrics relating to extensions.
    // Returns true if new metrics where recorded.
    virtual bool RecordExtensionsMetrics() = 0;
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Returns the day id for a given time for testing.
  static int GetDayIdForTesting(base::Time time);

  SupervisedUserMetricsService(
      PrefService* pref_service,
      SupervisedUserService& supervised_user_service,
      std::unique_ptr<SupervisedUserMetricsServiceExtensionDelegate>
          extensions_metrics_delegate);
  SupervisedUserMetricsService(const SupervisedUserMetricsService&) = delete;
  SupervisedUserMetricsService& operator=(const SupervisedUserMetricsService&) =
      delete;
  ~SupervisedUserMetricsService() override;

  // KeyedService:
  void Shutdown() override;

  // SupervisedUserServiceObserver:
  void OnURLFilterChanged() override;

 private:
  // Helper function to check if a new day has arrived.
  void CheckForNewDay();

  void EmitMetrics();
  // Clears cache of last recorded metrics. Subsequent `::EmitMetrics` will emit
  // all metrics.
  void ClearMetricsCache();

  const raw_ptr<PrefService> pref_service_;
  raw_ref<SupervisedUserService> supervised_user_service_;
  std::unique_ptr<SupervisedUserMetricsServiceExtensionDelegate>
      extensions_metrics_delegate_;
  // A periodic timer that checks if a new day has arrived.
  base::RepeatingTimer timer_;

  // Cache of last recorded values of SupervisedUserURLFilter to avoid
  // duplicated emissions.
  std::optional<WebFilterType> last_recorded_web_filter_type_;
  std::optional<SupervisedUserURLFilter::Statistics> last_recorded_statistics_;

  base::ScopedObservation<SupervisedUserService, SupervisedUserServiceObserver>
      supervised_user_service_observation_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_METRICS_SERVICE_H_
