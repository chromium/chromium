// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_MODEL_H_

#include <memory>
#include <string>
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Model to predict if a user switched devices.
class DeviceSwitcherModel : public DefaultModelProvider {
 public:
  // Any updates to these strings need to also update the field trials allowlist
  // in go/segmentation-field-trials-map.
  static constexpr char kAndroidPhoneLabel[] = "AndroidPhone";
  static constexpr char kIosPhoneChromeLabel[] = "IosPhoneChrome";
  static constexpr char kAndroidTabletLabel[] = "AndroidTablet";
  static constexpr char kIosTabletLabel[] = "IosTablet";
  static constexpr char kDesktopLabel[] = "Desktop";
  static constexpr char kOtherLabel[] = "Other";
  static constexpr char kSyncedAndFirstDeviceLabel[] = "SyncedAndFirstDevice";
  static constexpr char kNotSyncedLabel[] = "NotSynced";

  DeviceSwitcherModel();
  ~DeviceSwitcherModel() override = default;

  DeviceSwitcherModel(const DeviceSwitcherModel&) = delete;
  DeviceSwitcherModel& operator=(const DeviceSwitcherModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_MODEL_H_
