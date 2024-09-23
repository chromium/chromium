// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_RESULT_DISPATCHER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_RESULT_DISPATCHER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace segmentation_platform {

// The DeviceSwitcherResultDispatcher is a helper class for the DeviceSwitcher
// model. It is responsible for fetching classification results from pref, or
// waiting and updating new classification results if unavailable at request
// time.
class DeviceSwitcherResultDispatcher
    : public syncer::DeviceInfoTracker::Observer,
      public base::SupportsUserData::Data {
 public:
  DeviceSwitcherResultDispatcher(
      SegmentationPlatformService* segmentation_service,
      syncer::DeviceInfoTracker* device_info_tracker,
      PrefService* prefs,
      FieldTrialRegister* field_trial_register);
  ~DeviceSwitcherResultDispatcher() override;

  DeviceSwitcherResultDispatcher(const DeviceSwitcherResultDispatcher&) =
      delete;
  DeviceSwitcherResultDispatcher& operator=(
      const DeviceSwitcherResultDispatcher&) = delete;

  // Called to get the classification result synchronously. If none, returns
  // empty result.
  virtual ClassificationResult GetCachedClassificationResult();

  // Called to get the classification results from prefs if it exists, else it
  // will wait for results upto `timeout` and return when available. Handles only
  // one request at a time. On timeout, returns a kNotReady result.
  virtual void WaitForClassificationResult(
      base::TimeDelta timeout,
      ClassificationResultCallback callback);

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // DeviceInfoTracker::Observer impl:
  void OnDeviceInfoChange() override;

 private:
  void SaveResultToPref(const ClassificationResult& result);
  std::optional<ClassificationResult> ReadResultFromPref() const;

  void RefreshSegmentResultIfNeeded();
  void OnGotResult(const ClassificationResult& result);

  void OnWaitTimeout();

  void RegisterFieldTrials();

  const raw_ptr<SegmentationPlatformService> segmentation_service_;
  const raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<FieldTrialRegister> field_trial_register_;
  ClassificationResultCallback waiting_callback_;
  std::optional<ClassificationResult> latest_result_;

  // Note that the observation is only active when the result is not computed
  // yet.
  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      device_info_observation_{this};
  base::Time device_info_timestamp_;

  base::WeakPtrFactory<DeviceSwitcherResultDispatcher> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_RESULT_DISPATCHER_H_
