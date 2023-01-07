// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/pref_registry/pref_registry_syncable.h"

enum class PermissionAction;
enum class RequestType;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace permissions {
// This class records and stores all actions taken on permission prompts. It
// also has utility functions to interact with the history.
class PermissionActionsHistory : public KeyedService {
 public:
  struct Entry {
    PermissionAction action;
    base::Time time;

    bool operator==(const Entry that) const {
      return std::tie(this->action, this->time) ==
             std::tie(that.action, that.time);
    }
    bool operator!=(const Entry that) const { return !(*this == that); }
  };

  enum class EntryFilter {
    WANT_ALL_PROMPTS,
    WANT_LOUD_PROMPTS_ONLY,
    WANT_QUIET_PROMPTS_ONLY,
  };

  explicit PermissionActionsHistory(PrefService* pref_service);

  PermissionActionsHistory(const PermissionActionsHistory&) = delete;
  PermissionActionsHistory& operator=(const PermissionActionsHistory&) = delete;

  ~PermissionActionsHistory() override = default;

  // Get the history of recorded actions that happened after a particular time.
  // Optionally a permission request type can be specified which will only
  // return actions of that type.
  std::vector<Entry> GetHistory(const base::Time& begin,
                                EntryFilter entry_filter);
  std::vector<Entry> GetHistory(const base::Time& begin,
                                RequestType type,
                                EntryFilter entry_filter);

  // Record that a particular action has occurred at this time.
  // `base::Time::Now()` is used to retrieve the current time.
  void RecordAction(PermissionAction action,
                    RequestType type,
                    PermissionPromptDisposition prompt_disposition);

  // Delete logs of past user interactions. To be called when clearing
  // browsing data.
  void ClearHistory(const base::Time& delete_begin,
                    const base::Time& delete_end);

  PrefService* GetPrefServiceForTesting();

  static void FillInActionCounts(
      PredictionRequestFeatures::ActionCounts* counts,
      const std::vector<PermissionActionsHistory::Entry>& permission_actions);

  // Registers the preferences related to blocklisting in the given PrefService.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  std::vector<Entry> GetHistoryInternal(const base::Time& begin,
                                        const std::string& key,
                                        EntryFilter entry_filter);

  raw_ptr<PrefService> pref_service_;
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_H_
