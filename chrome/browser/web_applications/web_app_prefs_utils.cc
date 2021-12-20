// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_prefs_utils.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace web_app {

namespace {

const base::DictionaryValue* GetWebAppDictionary(
    const PrefService* pref_service,
    const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* web_apps_prefs =
      pref_service->GetDictionary(prefs::kWebAppsPreferences);
  if (!web_apps_prefs)
    return nullptr;

  const base::Value* web_app_prefs = web_apps_prefs->FindDictKey(app_id);
  if (!web_app_prefs)
    return nullptr;

  return &base::Value::AsDictionaryValue(*web_app_prefs);
}

std::unique_ptr<prefs::DictionaryValueUpdate> UpdateWebAppDictionary(
    std::unique_ptr<prefs::DictionaryValueUpdate> web_apps_prefs_update,
    const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs_update;
  if (!web_apps_prefs_update->GetDictionaryWithoutPathExpansion(
          app_id, &web_app_prefs_update)) {
    web_app_prefs_update =
        web_apps_prefs_update->SetDictionaryWithoutPathExpansion(
            app_id, std::make_unique<base::DictionaryValue>());
  }
  return web_app_prefs_update;
}

// Returns whether the time occurred within X days.
bool TimeOccurredWithinDays(absl::optional<base::Time> time, int days) {
  return time && (base::Time::Now() - time.value()).InDays() < days;
}

}  // namespace

// The stored preferences look like:
// "web_apps": {
//   "web_app_ids": {
//     "<app_id_1>": {
//       "was_external_app_uninstalled_by_user": true,
//       A double representing the number of seconds since epoch, in local time.
//       Convert from/to using base::Time::FromDoubleT() and
//       base::Time::ToDoubleT().
//       "IPH_num_of_consecutive_ignore": 2,
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "IPH_last_ignore_time": "13249617864945580",
//     },
//     "<app_id_N>": {
//       "was_external_app_uninstalled_by_user": false,
//     }
//   },
//   "app_agnostic_iph_state": {
//     "IPH_num_of_consecutive_ignore": 3,
//     A string-flavored base::Value representing int64_t number of microseconds
//     since the Windows epoch, using base::TimeToValue().
//     "IPH_last_ignore_time": "13249617864945500",
//   },
//   isolation_state is managed by isolation_prefs_utils
//   "isolation_state": {
//     "<origin>": {
//       "storage_isolation_key": "abc123",
//     },
//   },
// }
//
const char kWasExternalAppUninstalledByUser[] =
    "was_external_app_uninstalled_by_user";

const char kLatestWebAppInstallSource[] = "latest_web_app_install_source";

const char kIphIgnoreCount[] = "IPH_num_of_consecutive_ignore";

const char kIphLastIgnoreTime[] = "IPH_last_ignore_time";

void WebAppPrefsUtilsRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(::prefs::kWebAppsPreferences);
  registry->RegisterDictionaryPref(::prefs::kWebAppsAppAgnosticIphState);
}

bool GetBoolWebAppPref(const PrefService* pref_service,
                       const AppId& app_id,
                       base::StringPiece path) {
  if (const base::DictionaryValue* web_app_prefs =
          GetWebAppDictionary(pref_service, app_id)) {
    return web_app_prefs->FindBoolPath(path).value_or(false);
  }
  return false;
}

void UpdateBoolWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          bool value) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->SetBoolean(path, value);
}

absl::optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const AppId& app_id,
                                     base::StringPiece path) {
  const base::DictionaryValue* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs)
    return web_app_prefs->FindIntPath(path);
  return absl::nullopt;
}

void UpdateIntWebAppPref(PrefService* pref_service,
                         const AppId& app_id,
                         base::StringPiece path,
                         int value) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->SetInteger(path, value);
}

absl::optional<double> GetDoubleWebAppPref(const PrefService* pref_service,
                                           const AppId& app_id,
                                           base::StringPiece path) {
  const base::DictionaryValue* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs)
    return web_app_prefs->FindDoublePath(path);
  return absl::nullopt;
}

void UpdateDoubleWebAppPref(PrefService* pref_service,
                            const AppId& app_id,
                            base::StringPiece path,
                            double value) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->SetDouble(path, value);
}

absl::optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
                                             const AppId& app_id,
                                             base::StringPiece path) {
  if (const auto* web_app_prefs = GetWebAppDictionary(pref_service, app_id)) {
    if (auto* value = web_app_prefs->FindPath(path))
      return base::ValueToTime(value);
  }

  return absl::nullopt;
}

void UpdateTimeWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          base::Time value) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  auto web_app_prefs = UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->Set(path,
                     std::make_unique<base::Value>(base::TimeToValue(value)));
}

void RemoveWebAppPref(PrefService* pref_service,
                      const AppId& app_id,
                      base::StringPiece path) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->Remove(path);
}

absl::optional<int> GetWebAppInstallSource(PrefService* prefs,
                                           const AppId& app_id) {
  absl::optional<int> value =
      GetIntWebAppPref(prefs, app_id, kLatestWebAppInstallSource);
  DCHECK_GE(value.value_or(0), 0);
  DCHECK_LT(value.value_or(0),
            static_cast<int>(webapps::WebappInstallSource::COUNT));
  return value;
}

void UpdateWebAppInstallSource(PrefService* prefs,
                               const AppId& app_id,
                               int install_source) {
  DCHECK_GE(install_source, 0);
  DCHECK_LT(install_source,
            static_cast<int>(webapps::WebappInstallSource::COUNT));
  UpdateIntWebAppPref(prefs, app_id, kLatestWebAppInstallSource,
                      install_source);
}

void RecordInstallIphIgnored(PrefService* pref_service,
                             const AppId& app_id,
                             base::Time time) {
  absl::optional<int> ignored_count =
      GetIntWebAppPref(pref_service, app_id, kIphIgnoreCount);
  int new_count = base::saturated_cast<int>(1 + ignored_count.value_or(0));

  UpdateIntWebAppPref(pref_service, app_id, kIphIgnoreCount, new_count);
  UpdateTimeWebAppPref(pref_service, app_id, kIphLastIgnoreTime, time);

  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsAppAgnosticIphState);
  int global_count = 0;
  update->GetInteger(kIphIgnoreCount, &global_count);
  update->SetInteger(kIphIgnoreCount,
                     base::saturated_cast<int>(global_count + 1));
  update->Set(kIphLastIgnoreTime,
              std::make_unique<base::Value>(base::TimeToValue(time)));
}

void RecordInstallIphInstalled(PrefService* pref_service, const AppId& app_id) {
  // The ignored count is meant to track consecutive occurrences of the user
  // ignoring IPH, to help determine when IPH should be muted. Therefore
  // resetting ignored count on successful install.
  UpdateIntWebAppPref(pref_service, app_id, kIphIgnoreCount, 0);

  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsAppAgnosticIphState);
  update->SetInteger(kIphIgnoreCount, 0);
}

bool ShouldShowIph(PrefService* pref_service, const AppId& app_id) {
  // Do not show IPH if the user ignored the last N+ promos for this app.
  int app_ignored_count =
      GetIntWebAppPref(pref_service, app_id, kIphIgnoreCount).value_or(0);
  if (app_ignored_count >= kIphMuteAfterConsecutiveAppSpecificIgnores)
    return false;
  // Do not show IPH if the user ignored a promo for this app within N days.
  auto app_last_ignore =
      GetTimeWebAppPref(pref_service, app_id, kIphLastIgnoreTime);
  if (TimeOccurredWithinDays(app_last_ignore,
                             kIphAppSpecificMuteTimeSpanDays)) {
    return false;
  }

  auto* dict = pref_service->GetDictionary(prefs::kWebAppsAppAgnosticIphState);

  // Do not show IPH if the user ignored the last N+ promos for any app.
  int global_ignored_count = dict->FindIntKey(kIphIgnoreCount).value_or(0);
  if (global_ignored_count >= kIphMuteAfterConsecutiveAppAgnosticIgnores)
    return false;
  // Do not show IPH if the user ignored a promo for any app within N days.
  auto global_last_ignore =
      base::ValueToTime(dict->FindKey(kIphLastIgnoreTime));
  if (TimeOccurredWithinDays(global_last_ignore,
                             kIphAppAgnosticMuteTimeSpanDays)) {
    return false;
  }
  return true;
}

}  // namespace web_app
