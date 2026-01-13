// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_NOOP_IMPL_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_NOOP_IMPL_H_

#include "base/callback_list.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"

namespace supervised_user {

// An implementation of DeviceParentalControls intended for use on platforms
// that do not support parental controls. Offers browser-neutral behavior.
class DeviceParentalControlsNoOpImpl : public DeviceParentalControls {
 public:
  DeviceParentalControlsNoOpImpl();
  ~DeviceParentalControlsNoOpImpl() override;
  DeviceParentalControlsNoOpImpl(const DeviceParentalControlsNoOpImpl&) =
      delete;
  const DeviceParentalControlsNoOpImpl& operator=(
      const DeviceParentalControlsNoOpImpl&) = delete;

  // DeviceParentalControls:
  bool IsWebFilteringEnabled() const override;
  bool IsIncognitoModeDisabled() const override;
  bool IsSafeSearchForced() const override;
  bool IsEnabled() const override;
  void RegisterDeviceLevelSyntheticFieldTrials(
      SynteticFieldTrialDelegate& synthetic_field_trial_delegate)
      const override;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_DEVICE_PARENTAL_CONTROLS_NOOP_IMPL_H_
