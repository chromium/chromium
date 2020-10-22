// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_field_trial.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/ui/ui_features.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/tablet_mode.h"
#endif  // defined(OS_CHROMEOS)

// static
void WebUITabStripFieldTrial::RegisterFieldTrialIfNecessary() {
  static base::NoDestructor<WebUITabStripFieldTrial> instance;
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
#if defined(OS_CHROMEOS)
  return ash::TabletMode::IsBoardTypeMarkedAsTabletCapable();
#else
  // No known way to determine tablet-capability on other platforms.
  // Since returning true will record the synthetic field trial for all
  // devices, it'll do no better than the existing field trial. So,
  // return false and don't record this synthetic field trial.
  return false;
#endif  // defined(OS_CHROMEOS)
}
