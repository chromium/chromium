// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/permissions/permission_actions_history.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/blink/public/common/features.h"

namespace permissions {
namespace {

// Inner structure of |prefs::kPermissionActions| containing a history of past
// permission actions. It is a dictionary of JSON lists keyed on the result of
// PermissionUtil::GetPermissionString (lower-cased for backwards compatibility)
// and has the following format:
//
//   "profile.content_settings.permission_actions": {
//      "notifications": [
//       { "time": "1333333333337", "action": 1, "prompt_disposition": 2 },
//       { "time": "1567957177000", "action": 3, "prompt_disposition": 4 },
//     ],
//     "geolocation": [...],
//     ...
//   }
// The "prompt_disposition" key was added in M96. Any older entry will be
// missing that key. The value is backed by the PermissionPromptDisposition
// enum.
//
// Website settings are used to store data related to heuristic grants. This
// data is stored per-origin and per-permission type, and includes:
// - kTemporaryGrantCountKey: Stores the number of heuristic temporary grants
// for a permission.
// - kTemporaryGrantTimeStampKey: Stores the timestamp of the most recent
// temporary grant, including auto-grants.
constexpr char kPermissionActionEntryActionKey[] = "action";
constexpr char kPermissionActionEntryTimestampKey[] = "time";
constexpr char kPermissionActionEntryPromptDispositionKey[] =
    "prompt_disposition";

// Entries in permission actions expire after they become this old.
constexpr base::TimeDelta kPermissionActionMaxAge = base::Days(90);

// The threshold for temporary grants before a heuristic grant is made.
constexpr int kHeuristicGrantThreshold = 3;

// The duration after which the auto-grant expires.
constexpr base::TimeDelta kAutoGrantHeuristicallyExpiration = base::Days(28);

// Keys for storing data in website settings.
constexpr char kTemporaryGrantCountKey[] = "temporary_grant_count";
constexpr char kTemporaryGrantTimeStampKey[] = "temporary_grant_time_days";
constexpr char kOneTimeGrantCountKey[] = "one_time_grant_count";

base::Value::Dict GetOriginActionHistoryData(HostContentSettingsMap* settings,
                                             const GURL& origin_url) {
  base::Value website_setting = settings->GetWebsiteSetting(
      origin_url, GURL(), ContentSettingsType::PERMISSION_ACTIONS_HISTORY);
  if (!website_setting.is_dict()) {
    return base::Value::Dict();
  }

  return std::move(website_setting.GetDict());
}

base::Value::Dict* EnsurePermissionDict(base::Value::Dict& origin_dict,
                                        ContentSettingsType content_type) {
  // TODO(crbug.com/450467541): Support approximate location.
  CHECK((content_type == ContentSettingsType::GEOLOCATION ||
         content_type == ContentSettingsType::GEOLOCATION_WITH_OPTIONS) &&
        base::FeatureList::IsEnabled(blink::features::kGeolocationElement) &&
        base::FeatureList::IsEnabled(
            permissions::features::kPermissionHeuristicAutoGrant));
  return origin_dict.EnsureDict(
      PermissionUtil::GetPermissionString(content_type));
}

// Returns the current number of temporary grants recorded for `permission`
// type at `url`.
int GetTemporaryGrantCount(const GURL& url,
                           ContentSettingsType permission,
                           HostContentSettingsMap* settings_map) {
  base::Value::Dict dict = GetOriginActionHistoryData(settings_map, url);
  base::Value::Dict* permission_dict = EnsurePermissionDict(dict, permission);

  std::optional<int> value = permission_dict->FindInt(kTemporaryGrantCountKey);
  return value.value_or(0);
}

}  // namespace

PermissionActionsHistory::PermissionActionsHistory(
    PrefService* pref_service,
    HostContentSettingsMap* settings_map)
    : pref_service_(pref_service), settings_map_(settings_map) {}

PermissionActionsHistory::~PermissionActionsHistory() = default;

std::vector<PermissionActionsHistory::Entry>
PermissionActionsHistory::GetHistory(const base::Time& begin,
                                     EntryFilter entry_filter) {
  const base::Value::Dict& dictionary =
      pref_service_->GetDict(prefs::kPermissionActions);

  std::vector<PermissionActionsHistory::Entry> matching_actions;
  for (auto permission_entry : dictionary) {
    const auto permission_actions =
        GetHistoryInternal(begin, permission_entry.first, entry_filter);

    matching_actions.insert(matching_actions.end(), permission_actions.begin(),
                            permission_actions.end());
  }

  std::ranges::sort(
      matching_actions, {},
      [](const PermissionActionsHistory::Entry& entry) { return entry.time; });
  return matching_actions;
}

std::vector<PermissionActionsHistory::Entry>
PermissionActionsHistory::GetHistory(const base::Time& begin,
                                     RequestType type,
                                     EntryFilter entry_filter) {
  return GetHistoryInternal(begin, PermissionKeyForRequestType(type),
                            entry_filter);
}

void PermissionActionsHistory::RecordAction(
    PermissionAction action,
    RequestType type,
    PermissionPromptDisposition prompt_disposition) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kPermissionActions);
  base::Value::Dict& update_dict = update.Get();

  const std::string_view permission_path(PermissionKeyForRequestType(type));

  if (!update_dict.FindListByDottedPath(permission_path)) {
    update_dict.SetByDottedPath(permission_path, base::Value::List());
  }

  base::Value::List* permission_actions =
      update_dict.FindListByDottedPath(permission_path);
  CHECK(permission_actions);

  // Discard permission actions older than |kPermissionActionMaxAge|.
  const base::Time cutoff = base::Time::Now() - kPermissionActionMaxAge;
  permission_actions->EraseIf([cutoff](const base::Value& entry) {
    const std::optional<base::Time> timestamp = base::ValueToTime(
        entry.GetDict().Find(kPermissionActionEntryTimestampKey));
    return !timestamp || *timestamp < cutoff;
  });

  // Record the new permission action.
  base::Value::Dict new_action_attributes;
  new_action_attributes.Set(kPermissionActionEntryTimestampKey,
                            base::TimeToValue(base::Time::Now()));
  new_action_attributes.Set(kPermissionActionEntryActionKey,
                            static_cast<int>(action));
  new_action_attributes.Set(kPermissionActionEntryPromptDispositionKey,
                            static_cast<int>(prompt_disposition));
  permission_actions->Append(std::move(new_action_attributes));
}

void PermissionActionsHistory::ClearHistory(const base::Time& delete_begin,
                                            const base::Time& delete_end) {
  DCHECK(!delete_end.is_null());
  if (delete_begin.is_null() && delete_end.is_max()) {
    pref_service_->ClearPref(prefs::kPermissionActions);
    return;
  }

  ScopedDictPrefUpdate update(pref_service_, prefs::kPermissionActions);

  for (auto permission_entry : update.Get()) {
    permission_entry.second.GetList().EraseIf([delete_begin,
                                               delete_end](const auto& entry) {
      const std::optional<base::Time> timestamp = base::ValueToTime(
          entry.GetDict().Find(kPermissionActionEntryTimestampKey));
      return (!timestamp ||
              (*timestamp >= delete_begin && *timestamp < delete_end));
    });
  }
}

bool PermissionActionsHistory::RecordTemporaryGrant(
    const GURL& url,
    ContentSettingsType permission) {
  base::Value::Dict dict = GetOriginActionHistoryData(settings_map_, url);
  base::Value::Dict* permission_dict = EnsurePermissionDict(dict, permission);

  std::optional<int> value = permission_dict->FindInt(kTemporaryGrantCountKey);
  int current_count = value.value_or(0);

  permission_dict->Set(kTemporaryGrantCountKey, current_count + 1);
  permission_dict->Set(kTemporaryGrantTimeStampKey,
                       base::TimeToValue(base::Time::Now()));

  settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::PERMISSION_ACTIONS_HISTORY,
      base::Value(std::move(dict)));

  bool auto_granted = current_count >= kHeuristicGrantThreshold;
  if (auto_granted) {
    NotifyAutoGrantedHeuristically(url, permission);
  }
  return auto_granted;
}

void PermissionActionsHistory::ResetHeuristicData(
    const GURL& url,
    ContentSettingsType permission) {
  base::Value::Dict dict = GetOriginActionHistoryData(settings_map_, url);
  if (dict.empty()) {
    return;
  }

  dict.Remove(PermissionUtil::GetPermissionString(permission));
  settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::PERMISSION_ACTIONS_HISTORY,
      base::Value(std::move(dict)));
}

void PermissionActionsHistory::ResetHeuristicData(
    base::RepeatingCallback<bool(const GURL& url)> filter) {
  for (const auto& site : settings_map_->GetSettingsForOneType(
           ContentSettingsType::PERMISSION_ACTIONS_HISTORY)) {
    GURL origin(site.primary_pattern.ToString());

    if (origin.is_valid() && filter.Run(origin)) {
      settings_map_->SetWebsiteSettingDefaultScope(
          origin, GURL(), ContentSettingsType::PERMISSION_ACTIONS_HISTORY,
          base::Value());
    }
  }
}

bool PermissionActionsHistory::CheckHeuristicallyAutoGranted(
    const GURL& request_origin,
    ContentSettingsType permission,
    bool needs_update) {
  base::Value::Dict dict =
      GetOriginActionHistoryData(settings_map_, request_origin);
  base::Value::Dict* permission_dict = EnsurePermissionDict(dict, permission);

  std::optional<base::Time> last_grant_time =
      base::ValueToTime(permission_dict->Find(kTemporaryGrantTimeStampKey));
  std::optional<int> grant_count_opt =
      permission_dict->FindInt(kTemporaryGrantCountKey);
  int grant_count = grant_count_opt.value_or(0);

  // Check if the last grant has expired. If the grant has expired, decay the
  // count. If the count was at or above the threshold, it decays to 2.
  // Otherwise, the heuristic data is reset completely.
  if (last_grant_time.has_value() && base::Time::Now() - *last_grant_time >
                                         kAutoGrantHeuristicallyExpiration) {
    if (grant_count >= kHeuristicGrantThreshold) {
      permission_dict->Set(kTemporaryGrantCountKey,
                           kHeuristicGrantThreshold - 1);
      permission_dict->Set(kTemporaryGrantTimeStampKey,
                           base::TimeToValue(base::Time::Now()));
      settings_map_->SetWebsiteSettingDefaultScope(
          request_origin, GURL(),
          ContentSettingsType::PERMISSION_ACTIONS_HISTORY,
          base::Value(std::move(dict)));
    } else {
      ResetHeuristicData(request_origin, permission);
    }
    return false;
  }

  if (grant_count >= kHeuristicGrantThreshold) {
    if (needs_update) {
      permission_dict->Set(kTemporaryGrantTimeStampKey,
                           base::TimeToValue(base::Time::Now()));
      settings_map_->SetWebsiteSettingDefaultScope(
          request_origin, GURL(),
          ContentSettingsType::PERMISSION_ACTIONS_HISTORY,
          base::Value(std::move(dict)));
    }
    return true;
  }

  // The grant count is below the threshold, so it is not auto-granted.
  return false;
}

void PermissionActionsHistory::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void PermissionActionsHistory::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

int PermissionActionsHistory::GetTemporaryGrantCountForTesting(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return GetTemporaryGrantCount(request_origin, permission, settings_map_);
}

std::vector<PermissionActionsHistory::Entry>
PermissionActionsHistory::GetHistoryInternal(const base::Time& begin,
                                             const std::string& key,
                                             EntryFilter entry_filter) {
  const base::Value::List* permission_actions =
      pref_service_->GetDict(prefs::kPermissionActions).FindList(key);

  if (!permission_actions)
    return {};

  std::vector<Entry> matching_actions;

  for (const auto& entry : *permission_actions) {
    const base::Value::Dict& entry_dict = entry.GetDict();
    const std::optional<base::Time> timestamp =
        base::ValueToTime(entry_dict.Find(kPermissionActionEntryTimestampKey));

    if (timestamp < begin)
      continue;

    if (entry_filter != EntryFilter::WANT_ALL_PROMPTS) {
      // If we want either the Loud or Quiet UI actions but don't have this
      // info due to legacy reasons we ignore the entry.
      const std::optional<int> prompt_disposition_int =
          entry_dict.FindInt(kPermissionActionEntryPromptDispositionKey);
      if (!prompt_disposition_int)
        continue;

      const PermissionPromptDisposition prompt_disposition =
          static_cast<PermissionPromptDisposition>(*prompt_disposition_int);

      if (entry_filter == EntryFilter::WANT_LOUD_PROMPTS_ONLY &&
          !PermissionUmaUtil::IsPromptDispositionLoud(prompt_disposition)) {
        continue;
      }

      if (entry_filter == EntryFilter::WANT_QUIET_PROMPTS_ONLY &&
          !PermissionUmaUtil::IsPromptDispositionQuiet(prompt_disposition)) {
        continue;
      }
    }
    const PermissionAction past_action = static_cast<PermissionAction>(
        *(entry_dict.FindInt(kPermissionActionEntryActionKey)));
    matching_actions.emplace_back(
        PermissionActionsHistory::Entry{past_action, timestamp.value()});
  }
  return matching_actions;
}

void PermissionActionsHistory::NotifyAutoGrantedHeuristically(
    const GURL& origin,
    ContentSettingsType content_setting) {
  for (Observer& obs : observers_) {
    obs.OnAutoGrantedHeuristically(origin, content_setting);
  }
}

PrefService* PermissionActionsHistory::GetPrefServiceForTesting() {
  return pref_service_;
}

// static
void PermissionActionsHistory::FillInActionCounts(
    PredictionRequestFeatures::ActionCounts* counts,
    const std::vector<PermissionActionsHistory::Entry>& actions) {
  for (const auto& entry : actions) {
    switch (entry.action) {
      case PermissionAction::DENIED:
        counts->denies++;
        break;
      case PermissionAction::GRANTED:
      case PermissionAction::GRANTED_ONCE:
        counts->grants++;
        break;
      case PermissionAction::DISMISSED:
        counts->dismissals++;
        break;
      case PermissionAction::IGNORED:
        counts->ignores++;
        break;
      default:
        // Anything else is ignored.
        break;
    }
  }
}

void PermissionActionsHistory::RecordOneTimeGrant(
    const GURL& origin,
    ContentSettingsType permission_type) {
  if (permission_type != ContentSettingsType::GEOLOCATION &&
      permission_type != ContentSettingsType::MEDIASTREAM_MIC &&
      permission_type != ContentSettingsType::MEDIASTREAM_CAMERA) {
    return;
  }
  base::Value setting = settings_map_->GetWebsiteSetting(
      origin, origin, ContentSettingsType::PERMISSION_ACTIONS_HISTORY, nullptr);
  base::Value::Dict dict = !setting.is_none() && setting.is_dict()
                               ? setting.GetDict().Clone()
                               : base::Value::Dict();
  const std::string permission_str =
      PermissionUtil::GetPermissionString(permission_type);
  base::Value::Dict* permission_dict = dict.EnsureDict(permission_str);
  int current_count =
      permission_dict->FindInt(kOneTimeGrantCountKey).value_or(0);
  current_count++;
  permission_dict->Set(kOneTimeGrantCountKey, current_count);
  settings_map_->SetWebsiteSettingDefaultScope(
      origin, origin, ContentSettingsType::PERMISSION_ACTIONS_HISTORY,
      base::Value(std::move(dict)));
  base::UmaHistogramCounts100(
      "Permissions.OneTimePermission." + permission_str + ".OneTimeGrant",
      current_count);
}

void PermissionActionsHistory::RecordOTPCountForGrant(
    ContentSettingsType permission,
    int count) {
  if (permission != ContentSettingsType::GEOLOCATION &&
      permission != ContentSettingsType::MEDIASTREAM_MIC &&
      permission != ContentSettingsType::MEDIASTREAM_CAMERA) {
    return;
  }
  std::string histogram_name = "Permissions.OneTimePermission.";
  histogram_name += PermissionUtil::GetPermissionString(permission);
  histogram_name += ".GrantOTPCount";
  base::UmaHistogramCounts100(histogram_name, count);
}

int PermissionActionsHistory::GetOneTimeGrantCount(
    const GURL& origin,
    ContentSettingsType permission) {
  if (permission != ContentSettingsType::GEOLOCATION &&
      permission != ContentSettingsType::MEDIASTREAM_MIC &&
      permission != ContentSettingsType::MEDIASTREAM_CAMERA) {
    return 0;
  }
  base::Value setting = settings_map_->GetWebsiteSetting(
      origin, origin, ContentSettingsType::PERMISSION_ACTIONS_HISTORY, nullptr);
  if (!setting.is_none() && setting.is_dict()) {
    const base::Value::Dict& dict = setting.GetDict();
    const std::string permission_str =
        PermissionUtil::GetPermissionString(permission);
    const base::Value::Dict* permission_dict = dict.FindDict(permission_str);
    if (permission_dict) {
      return permission_dict->FindInt(kOneTimeGrantCountKey).value_or(0);
    }
  }
  return 0;
}

// static
void PermissionActionsHistory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kPermissionActions,
                                   PrefRegistry::LOSSY_PREF);
}

}  // namespace permissions
