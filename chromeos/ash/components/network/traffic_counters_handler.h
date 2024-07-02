// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TRAFFIC_COUNTERS_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TRAFFIC_COUNTERS_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"

namespace ash::traffic_counters {

// TrafficCountersHandler is a singleton, owned by ChromeBrowserMainPartsAsh.
// This class handles automatically resetting traffic counters in Shill on a
// date specified by the user. User specified auto reset days that are too
// large for a given month occur on the last day of that month. For example, if
// the user specified day was 29, the actual day of reset for the month of
// February would be February 28th on non-leap years and February 29th on leap
// years. Similarly, if the user specified day was 31, the actual day of reset
// for April would be April 30th.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) TrafficCountersHandler {
 public:
  // Sets the global instance and begins the work of the this class. Must be
  // called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static TrafficCountersHandler* Get();

  // Sets the global instance for this class. Used in testing only.
  static void InitializeForTesting();

  // Returns whether the global instance has been initialized.
  static bool IsInitialized();

  using TimeGetter = base::RepeatingCallback<base::Time()>;
  using RequestTrafficCountersCallback =
      base::OnceCallback<void(std::optional<base::Value> traffic_counters)>;
  using SetTrafficCountersResetDayCallback = base::OnceCallback<void(bool)>;

  TrafficCountersHandler(const TrafficCountersHandler&) = delete;
  TrafficCountersHandler& operator=(const TrafficCountersHandler&) = delete;

  // Requests traffic counters for |service_path|.
  void RequestTrafficCounters(const std::string& service_path,
                              RequestTrafficCountersCallback callback);

  // Resets traffic counters for |service_path|.
  void ResetTrafficCounters(const std::string& service_path);

  // Sets the traffic counters auto reset |day| for |guid|.
  void SetTrafficCountersResetDay(const std::string& guid,
                                  uint32_t day,
                                  SetTrafficCountersResetDayCallback callback);

  // Gets the auto reset specified day for |guid|.
  uint32_t GetUserSpecifiedResetDay(const std::string& guid);

  void SetTimeGetterForTesting(TimeGetter time_getter);
  void RunForTesting();
  void StartAutoResetForTesting();

 private:
  TrafficCountersHandler();
  ~TrafficCountersHandler();

  // Runs a check to determine whether traffic counters must be reset and starts
  // a timer to do so periodically.
  void StartAutoReset();
  void RunAutoResetTrafficCountersForActiveNetworks();
  void OnGetManagedPropertiesForAutoReset(
      std::string guid,
      const std::string& service_path,
      std::optional<base::Value::Dict> properties,
      std::optional<std::string> error);
  void OnGetManagedPropertiesForLastResetTime(
      double total_data_usage,
      const std::string& service_path,
      std::optional<base::Value::Dict> properties,
      std::optional<std::string> error);
  void OnTrafficCountersRequestedForTrackingDailyAverageDataUsage(
      const std::string& service_path,
      std::optional<base::Value> traffic_counters);
  // Returns a base::Time::Exploded object representing the current time. This
  // allows easy modification of the date values, which we can use to compare
  // different dates.
  base::Time::Exploded CurrentDateExploded();

  // Callback used to get the time. Mocked out in tests.
  TimeGetter time_getter_;
  // Timer used to set the interval at which to run auto reset checks.
  std::unique_ptr<base::RepeatingTimer> timer_;

  base::WeakPtrFactory<TrafficCountersHandler> weak_ptr_factory_{this};
};

}  // namespace ash::traffic_counters

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TRAFFIC_COUNTERS_HANDLER_H_
