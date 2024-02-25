// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_OS_SETTINGS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_OS_SETTINGS_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class PrefService;

namespace ash::settings {

class OsSettingsMetricsProvider : public metrics::MetricsProvider {
 public:
  OsSettingsMetricsProvider() = default;

  OsSettingsMetricsProvider(const OsSettingsMetricsProvider&) = delete;
  OsSettingsMetricsProvider& operator=(const OsSettingsMetricsProvider&) =
      delete;

  ~OsSettingsMetricsProvider() override = default;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  void LogVerifiedAccessEnabled();
  void MaybeLogTotalUniqueSettingsChanged();
  bool HasUserMetricsConsent();
  bool ShouldRecordMetrics(PrefService* profile_pref_service);
  bool IsTodayInFirst7Days(PrefService* profile_pref_service);
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_OS_SETTINGS_METRICS_PROVIDER_H_
