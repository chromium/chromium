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
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

namespace segmentation_platform {

// The DeviceSwitcherResultDispatcher is a helper class for the DeviceSwitcher
// model. It is responsible for fetching classification results from pref, or
// waiting and updating new classification results if unavailable at request
// time.
class DeviceSwitcherResultDispatcher : public base::SupportsUserData::Data,
                                       public syncer::SyncServiceObserver {
 public:
  DeviceSwitcherResultDispatcher(
      SegmentationPlatformService* segmentation_service,
      syncer::SyncService* sync_service,
      PrefService* prefs,
      FieldTrialRegister* field_trial_register);
  ~DeviceSwitcherResultDispatcher() override;

  DeviceSwitcherResultDispatcher(const DeviceSwitcherResultDispatcher&) =
      delete;
  DeviceSwitcherResultDispatcher& operator=(
      const DeviceSwitcherResultDispatcher&) = delete;

  // Called to get the classification result synchronously. If none, returns
  // empty result.
  ClassificationResult GetCachedClassificationResult();

  // Called to get the classification results from prefs if it exists, else it
  // will wait for results and return when available. Handles only one request
  // at a time.
  void WaitForClassificationResult(ClassificationResultCallback callback);

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SyncServiceObserver impl:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  void SaveResultToPref(const ClassificationResult& result);
  absl::optional<ClassificationResult> ReadResultFromPref();

  void RefreshSegmentResult();
  void OnGotResult(const ClassificationResult& result);

  void RegisterFieldTrials();

  const raw_ptr<SegmentationPlatformService> segmentation_service_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<FieldTrialRegister, DanglingUntriaged> field_trial_register_;
  ClassificationResultCallback waiting_callback_;
  absl::optional<ClassificationResult> latest_result_;

  // Observer for sync to record time durations. Note that the observation is
  // only active when needed for metrics.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  bool has_sync_consent_at_startup_{false};
  base::Time sync_consent_timestamp_;

  base::WeakPtrFactory<DeviceSwitcherResultDispatcher> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_RESULT_DISPATCHER_H_
