// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/daily_metrics_helper.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/numerics/clamped_math.h"
#include "base/value_iterators.h"
#include "base/values.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/date_changed_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/ukm/app_source_url_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace web_app {

namespace {

int BucketedDailySeconds(base::TimeDelta delta) {
  int64_t sample = std::clamp(delta.InSeconds(), static_cast<int64_t>(0),
                               base::Days(1).InSeconds());
  // Result between 1 sec and 1 day, in 50 linear buckets per day.
  int32_t bucket_size = base::Days(1).InSeconds() / 50;
  int result = ukm::GetLinearBucketMin(sample, bucket_size);
  return std::max(1, result);
}

}  // namespace

// This class exists just to be friended by `AppSourceUrlRecorder` to control
// the emission of web app AppKMs.
class DesktopWebAppUkmRecorder {
 public:
  static void Emit(const DailyInteraction& record) {
    DCHECK(record.start_url.is_valid());
    ukm::SourceId source_id =
        ukm::AppSourceUrlRecorder::GetSourceIdForPWA(record.start_url);
    ukm::builders::WebApp_DailyInteraction builder(source_id);
    builder.SetUsed(true)
        .SetInstalled(record.installed)
        .SetDisplayMode(record.effective_display_mode)
        .SetCapturesLinks(record.captures_links)
        .SetPromotable(record.promotable);
    if (record.install_source)
      builder.SetInstallSource(record.install_source.value());
    if (!record.foreground_duration.is_zero())
      builder.SetForegroundDuration(
          BucketedDailySeconds(record.foreground_duration));
    if (!record.background_duration.is_zero())
      builder.SetBackgroundDuration(
          BucketedDailySeconds(record.background_duration));
    if (record.num_sessions > 0)
      builder.SetNumSessions(record.num_sessions);
    builder.Record(ukm::UkmRecorder::Get());
    ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);
  }
};

namespace {

using std::optional;

bool skip_origin_check_for_testing_ = false;

const char kInstalled[] = "installed";
const char kInstallSource[] = "install_source";
const char kEffectiveDisplayMode[] = "effective_display_mode";
const char kCapturesLinks[] = "captures_links";
const char kPromotable[] = "promotable";
const char kForegroundDurationSec[] = "foreground_duration_sec";
const char kBackgroundDurationSec[] = "background_duration_sec";
const char kNumSessions[] = "num_sessions";

optional<DailyInteraction> DictToRecord(const std::string& url,
                                        const base::Value::Dict& record_dict) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return std::nullopt;
  DailyInteraction record(gurl);

  optional<int> installed = record_dict.FindBool(kInstalled);
  if (!installed.has_value())
    return std::nullopt;
  record.installed = *installed;

  record.install_source = record_dict.FindInt(kInstallSource);

  optional<int> effective_display_mode =
      record_dict.FindInt(kEffectiveDisplayMode);
  if (!effective_display_mode.has_value())
    return std::nullopt;
  record.effective_display_mode = *effective_display_mode;

  record.captures_links = record_dict.FindBool(kCapturesLinks).value_or(false);

  optional<bool> promotable = record_dict.FindBool(kPromotable);
  if (!promotable.has_value())
    return std::nullopt;
  record.promotable = *promotable;

  optional<int> foreground_duration_sec =
      record_dict.FindInt(kForegroundDurationSec);
  if (foreground_duration_sec) {
    record.foreground_duration = base::Seconds(*foreground_duration_sec);
  }

  optional<int> background_duration_sec =
      record_dict.FindInt(kBackgroundDurationSec);
  if (background_duration_sec) {
    record.background_duration = base::Seconds(*background_duration_sec);
  }

  optional<int> num_sessions = record_dict.FindInt(kNumSessions);
  if (num_sessions)
    record.num_sessions = *num_sessions;

  return record;
}

base::Value::Dict RecordToDict(DailyInteraction& record) {
  base::Value::Dict record_dict;
  // Note URL is not set here as it is the key for this dict in its parent.
  record_dict.Set(kInstalled, record.installed);
  if (record.install_source.has_value())
    record_dict.Set(kInstallSource, *record.install_source);
  record_dict.Set(kEffectiveDisplayMode, record.effective_display_mode);
  record_dict.Set(kCapturesLinks, record.captures_links);
  record_dict.Set(kPromotable, record.promotable);
  record_dict.Set(kForegroundDurationSec,
                  static_cast<int>(record.foreground_duration.InSeconds()));
  record_dict.Set(kBackgroundDurationSec,
                  static_cast<int>(record.background_duration.InSeconds()));
  record_dict.Set(kNumSessions, record.num_sessions);

  return record_dict;
}

void EmitIfSourceIdExists(DailyInteraction record,
                          optional<ukm::SourceId> origin_source_id) {
  if (!origin_source_id)
    return;

  // Make our own source ID that captures entire start_url, not just origin.
  DesktopWebAppUkmRecorder::Emit(record);
}

void EmitRecord(DailyInteraction record, Profile* profile) {
  if (skip_origin_check_for_testing_) {
    DesktopWebAppUkmRecorder::Emit(record);
    return;
  }
  auto* ukm_background_service =
      ukm::UkmBackgroundRecorderFactory::GetForProfile(profile);
  url::Origin origin = url::Origin::Create(record.start_url);
  // Ensure origin is still in the history before emitting.
  ukm_background_service->GetBackgroundSourceIdIfAllowed(
      origin, base::BindOnce(&EmitIfSourceIdExists, std::move(record)));
}

void EmitRecords(Profile* profile) {
  const base::Value::Dict& urls_to_features =
      profile->GetPrefs()->GetDict(prefs::kWebAppsDailyMetrics);

  for (const auto iter : urls_to_features) {
    const std::string& url = iter.first;
    const base::Value& val = iter.second;
    optional<DailyInteraction> record = DictToRecord(url, val.GetDict());
    if (record)
      EmitRecord(*record, profile);
  }
}

void RemoveRecords(PrefService* prefs) {
  prefs->SetDict(prefs::kWebAppsDailyMetrics, base::Value::Dict());
}

void UpdateRecord(DailyInteraction& record, PrefService* prefs) {
  DCHECK(record.start_url.is_valid());
  const std::string& url = record.start_url.spec();
  const base::Value::Dict& urls_to_features =
      prefs->GetDict(prefs::kWebAppsDailyMetrics);
  const base::Value::Dict* existing_val = urls_to_features.FindDict(url);
  if (existing_val) {
    // Sum duration and session values from existing record.
    optional<DailyInteraction> existing_record =
        DictToRecord(url, *existing_val);
    if (existing_record) {
      record.foreground_duration += existing_record->foreground_duration;
      record.background_duration += existing_record->background_duration;
      record.num_sessions += existing_record->num_sessions;
    }
  }

  base::Value::Dict record_dict = RecordToDict(record);
  ScopedDictPrefUpdate update(prefs, prefs::kWebAppsDailyMetrics);

  update->Set(url, std::move(record_dict));
}

}  // namespace

DailyInteraction::DailyInteraction() = default;
DailyInteraction::DailyInteraction(GURL start_url)
    : start_url(std::move(start_url)) {}
DailyInteraction::DailyInteraction(const DailyInteraction&) = default;
DailyInteraction::~DailyInteraction() = default;

void FlushOldRecordsAndUpdate(DailyInteraction& record, Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metrics::date_changed_helper::HasDateChangedSinceLastCall(
          profile->GetPrefs(), prefs::kWebAppsDailyMetricsDate)) {
    EmitRecords(profile);
    RemoveRecords(profile->GetPrefs());
  }
  UpdateRecord(record, profile->GetPrefs());
}

void FlushAllRecordsForTesting(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EmitRecords(profile);
  RemoveRecords(profile->GetPrefs());
}

void SkipOriginCheckForTesting() {
  skip_origin_check_for_testing_ = true;
}

void RegisterDailyWebAppMetricsProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kWebAppsDailyMetrics);
  metrics::date_changed_helper::RegisterPref(registry,
                                             prefs::kWebAppsDailyMetricsDate);
}

}  // namespace web_app
