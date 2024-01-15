// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/install_bounce_metric.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::optional<base::Time>& GetTimeOverride() {
  static std::optional<base::Time> time_override;
  return time_override;
}

base::Time GetTime() {
  if (!GetTimeOverride())
    return base::Time::Now();
  return *GetTimeOverride();
}

// TODO(alancutter): Dedupe Time/Value conversion logic with
// app_banner_settings_helper.cc and PrefService.
std::optional<base::Time> ParseTime(const base::Value* value) {
  if (!value || !value->is_string())
    return std::nullopt;

  int64_t integer;
  if (!base::StringToInt64(value->GetString(), &integer))
    return std::nullopt;

  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(integer));
}

base::Value SerializeTime(const base::Time& time) {
  return base::Value(
      base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMicroseconds()));
}

const char kInstallTimestamp[] = "install_timestamp";
const char kInstallSource[] = "install_source";

struct InstallMetrics {
  base::Time timestamp;
  webapps::WebappInstallSource source;
};

std::optional<InstallMetrics> ParseInstallMetricsFromPrefs(
    const PrefService* pref_service,
    const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::Dict& ids_to_metrics =
      pref_service->GetDict(prefs::kWebAppInstallMetrics);

  const base::Value::Dict* metrics = ids_to_metrics.FindDict(app_id);
  if (!metrics)
    return std::nullopt;

  std::optional<base::Time> timestamp =
      ParseTime(metrics->Find(kInstallTimestamp));
  if (!timestamp)
    return std::nullopt;

  const base::Value* source = metrics->Find(kInstallSource);
  if (!source || !source->is_int())
    return std::nullopt;

  return InstallMetrics{
      *timestamp, static_cast<webapps::WebappInstallSource>(source->GetInt())};
}

void WriteInstallMetricsToPrefs(const InstallMetrics& install_metrics,
                                PrefService* pref_service,
                                const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::Dict dict;
  dict.Set(kInstallTimestamp, SerializeTime(install_metrics.timestamp));
  dict.Set(kInstallSource, static_cast<int>(install_metrics.source));

  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppInstallMetrics);
  update->Set(app_id, std::move(dict));
}

}  // namespace

namespace web_app {

void SetInstallBounceMetricTimeForTesting(std::optional<base::Time> time) {
  GetTimeOverride() = time;
}

void RegisterInstallBounceMetricProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kWebAppInstallMetrics);
}

void RecordWebAppInstallationTimestamp(
    PrefService* pref_service,
    const webapps::AppId& app_id,
    webapps::WebappInstallSource install_source) {
  WriteInstallMetricsToPrefs(InstallMetrics{GetTime(), install_source},
                             pref_service, app_id);
}

void RecordWebAppUninstallation(PrefService* pref_service,
                                const webapps::AppId& app_id) {
  std::optional<InstallMetrics> metrics =
      ParseInstallMetricsFromPrefs(pref_service, app_id);
  if (!metrics)
    return;

  constexpr base::TimeDelta kMaxInstallBounceDuration = base::Hours(1);
  if (GetTime() - metrics->timestamp > kMaxInstallBounceDuration)
    return;

  UMA_HISTOGRAM_ENUMERATION("Webapp.Install.InstallBounce", metrics->source,
                            webapps::WebappInstallSource::COUNT);
}

}  // namespace web_app
