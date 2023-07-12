// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "components/variations/variations_switches.h"
#include "ui/base/device_form_factor.h"

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

  // Return the embedder-provided channel if no forced channel is specified.
  return GetChannel();
}

Study::FormFactor VariationsServiceClient::GetCurrentFormFactor() {
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
  }
  NOTREACHED();
  return Study::DESKTOP;
#endif  // BUILDFLAG(PLATFORM_CFM)
}

std::unique_ptr<SeedResponse>
VariationsServiceClient::TakeSeedFromNativeVariationsSeedStore() {
  return nullptr;
}

}  // namespace variations
