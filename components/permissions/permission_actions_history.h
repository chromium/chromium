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
class HostContentSettingsMap;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace permissions {
// This class records and stores all actions related to permission prompts in a
// global, browser-wide scope, offering utility functions for history
// interaction. Additionally, it manages heuristic permission grants,
// specifically within origin scopes, based on repeated temporary grants. This
// feature aims to reduce user friction for those who frequently grant temporary
// permissions (e.g., "Allow this time") to a site for a particular permission.
class PermissionActionsHistory : public KeyedService {
 public:
  struct Entry {
    PermissionAction action;
    base::Time time;

    friend bool operator==(const Entry&, const Entry&) = default;
  };

  enum class EntryFilter {
    WANT_ALL_PROMPTS,
    WANT_LOUD_PROMPTS_ONLY,
    WANT_QUIET_PROMPTS_ONLY,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAutoGrantedHeuristically(
        const GURL& origin,
        ContentSettingsType content_setting) = 0;
  };

  explicit PermissionActionsHistory(PrefService* pref_service,
                                    HostContentSettingsMap* settings_map);

  PermissionActionsHistory(const PermissionActionsHistory&) = delete;
  PermissionActionsHistory& operator=(const PermissionActionsHistory&) = delete;

  ~PermissionActionsHistory() override;

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

  // Incrementally records the number of temporary grants and the timestamp of
  // the most recent temporary grant for a given `permission` type at
  // `url` by one. Returns `true` if the counter reaches the
  // threshold and auto-grant is activated; otherwise, it returns `false`.
  bool RecordTemporaryGrant(const GURL& url, ContentSettingsType permission);

  // Resets the heuristic data for the given URL and permission, for
  // example when the user manually resets permissions.
  void ResetHeuristicData(const GURL& url, ContentSettingsType permission);

  // Same as above, but cleans the slate for all permissions and for all URLs
  // matching |filter|.
  void ResetHeuristicData(
      base::RepeatingCallback<bool(const GURL& url)> filter);

  // Checks if a permission has been heuristically auto-granted. If
  // `needs_update` is true, update the auto-granted stored data if exists.
  // `needs_update` is expected to be true, it is only set `false` from test
  // code.
  bool CheckHeuristicallyAutoGranted(const GURL& request_origin,
                                     ContentSettingsType permission,
                                     bool needs_update = true);

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  int GetTemporaryGrantCountForTesting(const GURL& request_origin,
                                       ContentSettingsType permission);

  // Records a one-time grant for the given origin and permission type.
  void RecordOneTimeGrant(const GURL& origin,
                          ContentSettingsType permission_type);

  // Records the number of times a one-time permission was granted before a
  // permanent grant.
  void RecordOTPCountForGrant(ContentSettingsType permission, int count);

  // Returns the number of times a one-time permission has been granted for the
  // given origin and permission type.
  int GetOneTimeGrantCount(const GURL& origin, ContentSettingsType permission);

 private:
  std::vector<Entry> GetHistoryInternal(const base::Time& begin,
                                        const std::string& key,
                                        EntryFilter entry_filter);

  void NotifyAutoGrantedHeuristically(const GURL& origin,
                                      ContentSettingsType content_setting);

  raw_ptr<PrefService> pref_service_;

  raw_ptr<HostContentSettingsMap> settings_map_;

  base::ObserverList<Observer> observers_;
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_H_
