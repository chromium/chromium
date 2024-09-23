// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/permissions/permission_actions_history.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/adapters.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

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
constexpr char kPermissionActionEntryActionKey[] = "action";
constexpr char kPermissionActionEntryTimestampKey[] = "time";
constexpr char kPermissionActionEntryPromptDispositionKey[] =
    "prompt_disposition";

// Entries in permission actions expire after they become this old.
constexpr base::TimeDelta kPermissionActionMaxAge = base::Days(90);

}  // namespace

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

  base::ranges::sort(
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

PermissionActionsHistory::PermissionActionsHistory(PrefService* pref_service)
    : pref_service_(pref_service) {}

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

// static
void PermissionActionsHistory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kPermissionActions,
                                   PrefRegistry::LOSSY_PREF);
}

}  // namespace permissions
