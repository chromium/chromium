// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_settings_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace ash::settings {

namespace {

constexpr char kOsSettingsVerifiedAccessEnabledHistogramName[] =
    "ChromeOS.Settings.Privacy.VerifiedAccessEnabled";

}  // namespace

void OsSettingsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  // Log verified access enabled/disabled value for this session
  bool verified_access_enabled;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kAttestationForContentProtectionEnabled, &verified_access_enabled);
  base::UmaHistogramBoolean(kOsSettingsVerifiedAccessEnabledHistogramName,
                            verified_access_enabled);
}

}  // namespace ash::settings
