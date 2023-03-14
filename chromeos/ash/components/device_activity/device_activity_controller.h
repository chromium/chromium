// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/device_activity/churn_active_status.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefRegistrySimple;
class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::device_activity {

class DeviceActivityClient;

struct ChromeDeviceMetadataParameters;

// Counts device actives in a privacy compliant way.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    DeviceActivityController {
 public:
  // Retrieves a singleton instance.
  static DeviceActivityController* Get();

  // Registers local state preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Determines the total start up delay before starting device activity
  // reporting.
  static base::TimeDelta DetermineStartUpDelay(base::Time chrome_first_run_ts);

  // Determines the market segment from the loaded ChromeOS device policies.
  static MarketSegment GetMarketSegment(
      policy::DeviceMode device_mode,
      policy::MarketSegment device_market_segment);

  DeviceActivityController(
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Time chrome_first_run_time,
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback);
  DeviceActivityController(const DeviceActivityController&) = delete;
  DeviceActivityController& operator=(const DeviceActivityController&) = delete;
  ~DeviceActivityController();

  // Testing methods to validate behaviour of class.
  int GetRetryOobeCompletedCountForTesting() const;
  base::OneShotTimer* GetOobeCompletedTimerForTesting();

 private:
  // Start Device Activity reporting.
  void Start(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback);

  // Stop Device Activity reporting.
  void Stop();

  // Wrapper method for the PostTaskAndReplyWithResult, which is used to spawn
  // a worker thread to check oobe completed file time delta.
  void CheckOobeCompletedInWorker(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback);

  // Retry method every kOobeReadFailedRetryDelay minute until confirming
  // 1 minute has passed since /home/chronos/.oobe_completed file was written.
  // Maximum retry count is kNumberOfRetriesBeforeFail.
  void OnOobeFileWritten(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback,
      base::TimeDelta time_since_oobe_file_written);

  void OnPsmDeviceActiveSecretFetched(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& psm_device_active_secret);

  void OnMachineStatisticsLoaded(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& psm_device_active_secret);

  // ChurnActiveStatus object stores the active history for the device.
  // This bit value will be persisted in local state and preserved files.
  ChurnActiveStatus churn_active_status_;

  // Reads the creation time of the first run sentinel file. If the first run
  // sentinel file does not exist, it will return base::Time().
  const base::Time chrome_first_run_time_;

  // Creates a copy of chrome parameters, which is owned throughout
  // |DeviceActivityController| object lifetime.
  const ChromeDeviceMetadataParameters chrome_passed_device_params_;

  // Singleton lives throughout class lifetime.
  system::StatisticsProvider* const statistics_provider_;

  // Number of retry attempts at reading the oobe completed file.
  int retry_oobe_completed_count_ = 0;

  // Timer used to retry reading oobe_completed file.
  std::unique_ptr<base::OneShotTimer> oobe_completed_timer_;

  std::unique_ptr<DeviceActivityClient> da_client_network_;

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<DeviceActivityController> weak_factory_{this};
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_
