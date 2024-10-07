// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "components/variations/variations_switches.h"
#include "ui/base/device_form_factor.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace variations {

version_info::Channel VariationsServiceClient::GetChannelForVariations() {
  const std::string forced_channel =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFakeVariationsChannel);
  if (!forced_channel.empty()) {
    if (forced_channel == "stable")
      return version_info::Channel::STABLE;
    if (forced_channel == "beta")
      return version_info::Channel::BETA;
    if (forced_channel == "dev")
      return version_info::Channel::DEV;
    if (forced_channel == "canary")
      return version_info::Channel::CANARY;
    DVLOG(1) << "Invalid channel provided: " << forced_channel;
  }

  auto channel = GetChannel();
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40936710): Remove this if block after automotive beta ends.
  if (channel == version_info::Channel::BETA &&
      base::android::BuildInfo::GetInstance()->is_automotive()) {
    return version_info::Channel::STABLE;
  }
#endif
  // Return the embedder-provided channel if no forced channel is specified.
  return channel;
}

Study::FormFactor VariationsServiceClient::GetCurrentFormFactor() {
// Temporary workaround to report foldable for variations without affecting
// other form factors. This will be removed and replaced with a long-term
// solution in DeviceFormFactor::GetDeviceFormFactor() after conducting an
// audit of form factor usage or exposing ui_mode.
// FormFactorMetricsProvider::GetFormFactor() also needs to be updated.
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_foldable()) {
    return Study::FOLDABLE;
  }
#endif

#if BUILDFLAG(PLATFORM_CFM)
  return Study::MEET_DEVICE;
#else
  switch (ui::GetDeviceFormFactor()) {
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return Study::PHONE;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return Study::TABLET;
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return Study::DESKTOP;
    case ui::DEVICE_FORM_FACTOR_TV:
      return Study::TV;
    case ui::DEVICE_FORM_FACTOR_AUTOMOTIVE:
      return Study::AUTOMOTIVE;
    case ui::DEVICE_FORM_FACTOR_FOLDABLE:
      return Study::FOLDABLE;
  }
  NOTREACHED_IN_MIGRATION();
  return Study::DESKTOP;
#endif  // BUILDFLAG(PLATFORM_CFM)
}

base::FilePath VariationsServiceClient::GetVariationsSeedFileDir() {
  return base::FilePath();
}

std::unique_ptr<SeedResponse>
VariationsServiceClient::TakeSeedFromNativeVariationsSeedStore() {
  return nullptr;
}

}  // namespace variations
