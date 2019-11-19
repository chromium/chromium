// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_bounce_metric.h"

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace {

base::Optional<base::Time>& GetTimeOverride() {
  static base::NoDestructor<base::Optional<base::Time>> g_time_override;
  return *g_time_override;
}

base::Time GetTime() {
  if (!GetTimeOverride())
    return base::Time::Now();
  return *GetTimeOverride();
}

// TODO(alancutter): Dedupe Time/Value conversion logic with
// app_banner_settings_helper.cc and PrefService.
base::Optional<base::Time> ParseTime(const base::Value* value) {
  std::string delta_string;
  if (!value || !value->GetAsString(&delta_string))
    return base::nullopt;

  int64_t integer;
  if (!base::StringToInt64(delta_string, &integer))
    return base::nullopt;

  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(integer));
}

base::Value SerializeTime(const base::Time& time) {
  return base::Value(
      base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMicroseconds()));
}

const char kInstallTimestamp[] = "install_timestamp";
const char kInstallSource[] = "install_source";

struct InstallMetrics {
  base::Time timestamp;
  WebappInstallSource source;
};

base::Optional<InstallMetrics> ParseInstallMetricsFromPrefs(
    const PrefService* pref_service,
    const web_app::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::DictionaryValue* ids_to_metrics =
      pref_service->GetDictionary(prefs::kWebAppInstallMetrics);
  if (!ids_to_metrics)
    return base::nullopt;

  const base::Value* metrics = ids_to_metrics->FindDictKey(app_id);
  if (!metrics)
    return base::nullopt;

  base::Optional<base::Time> timestamp =
      ParseTime(metrics->FindKey(kInstallTimestamp));
  if (!timestamp)
    return base::nullopt;

  const base::Value* source = metrics->FindKey(kInstallSource);
  if (!source || !source->is_int())
    return base::nullopt;

  return InstallMetrics{*timestamp,
                        static_cast<WebappInstallSource>(source->GetInt())};
}

void WriteInstallMetricsToPrefs(const InstallMetrics& install_metrics,
                                PrefService* pref_service,
                                const web_app::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kInstallTimestamp, SerializeTime(install_metrics.timestamp));
  dict.SetKey(kInstallSource,
              base::Value(static_cast<int>(install_metrics.source)));

  DictionaryPrefUpdate update(pref_service, prefs::kWebAppInstallMetrics);
  update->SetKey(app_id, std::move(dict));
}

}  // namespace

namespace web_app {

void SetInstallBounceMetricTimeForTesting(base::Optional<base::Time> time) {
  GetTimeOverride() = time;
}

void RegisterInstallBounceMetricProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kWebAppInstallMetrics);
}

void RecordWebAppInstallationTimestamp(PrefService* pref_service,
                                       const AppId& app_id,
                                       WebappInstallSource install_source) {
  WriteInstallMetricsToPrefs(InstallMetrics{GetTime(), install_source},
                             pref_service, app_id);
}

void RecordWebAppUninstallation(PrefService* pref_service,
                                const AppId& app_id) {
  base::Optional<InstallMetrics> metrics =
      ParseInstallMetricsFromPrefs(pref_service, app_id);
  if (!metrics)
    return;

  constexpr base::TimeDelta kMaxInstallBounceDuration =
      base::TimeDelta::FromHours(1);
  if (GetTime() - metrics->timestamp > kMaxInstallBounceDuration)
    return;

  UMA_HISTOGRAM_ENUMERATION("Webapp.Install.InstallBounce", metrics->source,
                            WebappInstallSource::COUNT);
}

}  // namespace web_app
