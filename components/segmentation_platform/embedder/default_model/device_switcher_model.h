// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_MODEL_H_

#include <memory>
#include <string>
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
constexpr char kAndroidPhoneLabel[] = "AndroidPhone";
constexpr char kIosPhoneChromeLabel[] = "IosPhoneChrome";
constexpr char kAndroidTabletLabel[] = "AndroidTablet";
constexpr char kIosTabletLabel[] = "IosTablet";
constexpr char kDesktopLabel[] = "Desktop";
constexpr char kOtherLabel[] = "Other";
constexpr char kSyncedAndFirstDeviceLabel[] = "SyncedAndFirstDevice";
constexpr char kNotSyncedLabel[] = "NotSynced";

}  // namespace

struct Config;

// Model to predict if a user switched devices.
class DeviceSwitcherModel : public ModelProvider {
 public:
  DeviceSwitcherModel();
  ~DeviceSwitcherModel() override = default;

  DeviceSwitcherModel(const DeviceSwitcherModel&) = delete;
  DeviceSwitcherModel& operator=(const DeviceSwitcherModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEVICE_SWITCHER_MODEL_H_
