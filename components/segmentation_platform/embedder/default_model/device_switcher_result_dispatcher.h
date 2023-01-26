// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_RESULT_DISPATCHER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_RESULT_DISPATCHER_H_

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

// The DeviceSwitcherResultDispatcher is a helper class for the DeviceSwitcher
// model. It is responsible for fetching classification results from pref, or
// waiting and updating new classification results if unavailable at request
// time.
class DeviceSwitcherResultDispatcher : public base::SupportsUserData::Data {
 public:
  DeviceSwitcherResultDispatcher(
      SegmentationPlatformService* segmentation_service,
      PrefService* prefs,
      FieldTrialRegister* field_trial_register);
  ~DeviceSwitcherResultDispatcher() override;

  DeviceSwitcherResultDispatcher(const DeviceSwitcherResultDispatcher&) =
      delete;
  DeviceSwitcherResultDispatcher& operator=(
      const DeviceSwitcherResultDispatcher&) = delete;

  // Called to get the classification results from prefs if it exists, else it
  // will wait for results and return when available. Handles only one request
  // at a time.
  void GetClassificationResult(ClassificationResultCallback callback);

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  void SaveResultToPref(const ClassificationResult& result);
  absl::optional<ClassificationResult> ReadResultFromPref();

  void OnGotResult(const ClassificationResult& result);

  const raw_ptr<SegmentationPlatformService> segmentation_service_;
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<FieldTrialRegister> field_trial_register_;
  ClassificationResultCallback waiting_callback_;
  bool initialized_{false};

  base::WeakPtrFactory<DeviceSwitcherResultDispatcher> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_RESULT_DISPATCHER_H_