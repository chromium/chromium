// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_field_trial.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/ui/ui_features.h"

// Platform-specific headers for detecting tablet devices.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/tablet_mode.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

#if BUILDFLAG(IS_WIN)

// Returns whether the screen supports touch input. Returns false if it
// couldn't be checked.
bool HasBuiltInTouchScreen() {
  int result = ::GetSystemMetrics(SM_DIGITIZER);

  // The |result| can be a combination of the following:
  // - NID_INTEGRATED_TOUCH: built-in touch screen
  // - NID_EXTERNAL_TOUCH: external touch screen or tablet
  // - NID_INTEGRATED_PEN: built-in pen support
  // - NID_EXTERNAL_PEN: external pen screen or tablet
  // - NID_MULTI_INPUT: not sure what this means
  // - NID_READY: currently ready to receive touch or pen input
  //
  // Ideally, we want to determine if a device is a tablet or
  // convertible. This means it supports entering system tablet mode.
  // There is no direct way to check this, however having a built-in
  // touch screen is a good proxy. Ignore pen digitizers; if a device
  // only supports pen input it probably isn't a tablet.

  return (result & NID_INTEGRATED_TOUCH) != 0;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

// static
void WebUITabStripFieldTrial::RegisterFieldTrialIfNecessary() {
  static WebUITabStripFieldTrial instance;
}

WebUITabStripFieldTrial::WebUITabStripFieldTrial() {
  if (!DeviceIsTabletModeCapable())
    return;

  base::FeatureList* const feature_list = base::FeatureList::GetInstance();

  if (feature_list->IsFeatureOverriddenFromCommandLine(
          features::kWebUITabStrip.name))
    return;

  const char* group_name;

  if (!feature_list->IsFeatureOverridden(features::kWebUITabStrip.name))
    group_name = "Default";
  else if (base::FeatureList::IsEnabled(features::kWebUITabStrip))
    group_name = "Enabled";
  else
    group_name = "Disabled";

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "WebUITabStripOnTablets", group_name);
}

// static
bool WebUITabStripFieldTrial::DeviceIsTabletModeCapable() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::TabletMode::IsBoardTypeMarkedAsTabletCapable();
#elif BUILDFLAG(IS_WIN)
  return HasBuiltInTouchScreen();
#else
  // No known way to determine tablet-capability on other platforms.
  // Since returning true will record the synthetic field trial for all
  // devices, it'll do no better than the existing field trial. So,
  // return false and don't record this synthetic field trial.
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
