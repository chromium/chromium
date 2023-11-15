// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/ui/form_factor_metrics_provider.h"
#include "build/config/chromebox_for_meetings/buildflags.h"  // PLATFORM_CFM

#include "build/build_config.h"
#include "ui/base/device_form_factor.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace metrics {

void FormFactorMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  system_profile_proto->mutable_hardware()->set_form_factor(GetFormFactor());
}

SystemProfileProto::Hardware::FormFactor
FormFactorMetricsProvider::GetFormFactor() const {
// Temporary workaround to report foldable for UMA without affecting
// other form factors. This will be removed and replaced with a long-term
// solution in DeviceFormFactor::GetDeviceFormFactor() after conducting an
// audit of form factor usage or exposing ui_mode.
// VariationsServiceClient::GetCurrentFormFactor() also needs to be updated.
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_foldable()) {
    return SystemProfileProto::Hardware::FORM_FACTOR_FOLDABLE;
  }
#endif

#if BUILDFLAG(PLATFORM_CFM)
  return SystemProfileProto::Hardware::FORM_FACTOR_MEET_DEVICE;
#else
  switch (ui::GetDeviceFormFactor()) {
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return SystemProfileProto::Hardware::FORM_FACTOR_DESKTOP;
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return SystemProfileProto::Hardware::FORM_FACTOR_PHONE;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return SystemProfileProto::Hardware::FORM_FACTOR_TABLET;
    case ui::DEVICE_FORM_FACTOR_TV:
      return SystemProfileProto::Hardware::FORM_FACTOR_TV;
    case ui::DEVICE_FORM_FACTOR_AUTOMOTIVE:
      return SystemProfileProto::Hardware::FORM_FACTOR_AUTOMOTIVE;
    case ui::DEVICE_FORM_FACTOR_FOLDABLE:
      return SystemProfileProto::Hardware::FORM_FACTOR_FOLDABLE;
    default:
      return SystemProfileProto::Hardware::FORM_FACTOR_UNKNOWN;
  }
#endif  // BUILDFLAG(PLATFORM_CFM)
}

}  // namespace metrics
