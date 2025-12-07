// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_client.h"

#include <cstdio>
#include <cstdlib>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "components/variations/variations_switches.h"
#include "ui/base/device_form_factor.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace variations {

version_info::Channel VariationsServiceClient::GetChannelForVariations() {
  const std::string forced_channel =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFakeVariationsChannel);
  if (!forced_channel.empty()) {
    if (forced_channel == "stable") {
      return version_info::Channel::STABLE;
    }
    if (forced_channel == "beta") {
      return version_info::Channel::BETA;
    }
    if (forced_channel == "dev") {
      return version_info::Channel::DEV;
    }
    if (forced_channel == "canary") {
      return version_info::Channel::CANARY;
    }
    DVLOG(1) << "Invalid channel provided: " << forced_channel;
  }

  auto channel = GetChannel();
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/389565104): Remove this if block when ready to move desktop
  // to stable builds.
  if (channel == version_info::Channel::STABLE &&
      base::android::device_info::is_desktop()) {
    return version_info::Channel::DEV;
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
  if (base::android::device_info::is_foldable()) {
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
    // TODO(crbug.com/435473340) Since XR study is not established yet for UMA.
    // To prevent compilation failure temporarily using tablet as it closer to
    // the tablet form factor. XR devices are not public yet.
    case ui::DEVICE_FORM_FACTOR_XR:
      return Study::TABLET;
  }
  NOTREACHED();
#endif  // BUILDFLAG(PLATFORM_CFM)
}

base::FilePath VariationsServiceClient::GetVariationsSeedFileDir() {
  return base::FilePath();
}

std::unique_ptr<SeedResponse>
VariationsServiceClient::TakeSeedFromNativeVariationsSeedStore() {
  return nullptr;
}

void VariationsServiceClient::ExitWithMessage(const std::string& message) {
  UNSAFE_TODO(puts(message.c_str()));
  exit(1);
}

bool VariationsServiceClient::IsStickyActivationEnabled() {
  return true;
}

}  // namespace variations
