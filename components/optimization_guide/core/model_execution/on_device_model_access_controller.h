// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ACCESS_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ACCESS_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"

class PrefService;

namespace optimization_guide {

enum class OnDeviceModelEligibilityReason;

// OnDeviceModelAccessController determines when the model may be used.
// If the model repeatedly crashes, or the gpu is blocked, then
// OnDeviceModelAccessController will disallow usage of the model.
// OnDeviceModelAccessController stores its state in prefs.
class OnDeviceModelAccessController {
 public:
  explicit OnDeviceModelAccessController(PrefService& pref_service);
  ~OnDeviceModelAccessController();

  // Returns success if a new session should be started.
  OnDeviceModelEligibilityReason ShouldStartNewSession() const;

  // Called when the complete response was received.
  void OnResponseCompleted();

  // Called when a connection from the remote happens prematurely.
  void OnDisconnectedFromRemote();

  // Called if using the gpu is blocked.
  void OnGpuBlocked();

  // Called if the session times out.
  void OnSessionTimedOut();

  bool ShouldValidateModel(std::string_view model_version);
  void OnValidationFinished(OnDeviceModelValidationResult result);

 private:
  struct ValidationState {
    std::string component_version;
    OnDeviceModelValidationResult result =
        OnDeviceModelValidationResult::kUnknown;
    int attempt_count = 0;
  };
  ValidationState GetValidationState() const;
  void SetValidationState(const ValidationState& state);

  raw_ref<PrefService> pref_service_;
  bool is_gpu_blocked_ = false;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ACCESS_CONTROLLER_H_
