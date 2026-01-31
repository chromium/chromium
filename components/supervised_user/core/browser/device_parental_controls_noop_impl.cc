// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/device_parental_controls_noop_impl.h"

#include "base/callback_list.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"

namespace supervised_user {

DeviceParentalControlsNoOpImpl::DeviceParentalControlsNoOpImpl() = default;
DeviceParentalControlsNoOpImpl::~DeviceParentalControlsNoOpImpl() = default;

bool DeviceParentalControlsNoOpImpl::IsWebFilteringEnabled() const {
  return false;
}

bool DeviceParentalControlsNoOpImpl::IsIncognitoModeDisabled() const {
  return false;
}

bool DeviceParentalControlsNoOpImpl::IsSafeSearchForced() const {
  return false;
}

bool DeviceParentalControlsNoOpImpl::IsEnabled() const {
  return false;
}

void DeviceParentalControlsNoOpImpl::RegisterDeviceLevelSyntheticFieldTrials(
    SynteticFieldTrialDelegate& synthetic_field_trial_delegate) const {}
}  // namespace supervised_user
