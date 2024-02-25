// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_

#include <optional>
#include <set>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/permission_result.h"
#include "url/gurl.h"

class GURL;

namespace permissions {

// Mockable interface for PermissionDecisionAutoBlocker (see below), for those
// few instances where this is used outside of permissions and needs separate
// unit tests.
class PermissionDecisionAutoBlockerBase {
 public:
  PermissionDecisionAutoBlockerBase() = default;
  virtual ~PermissionDecisionAutoBlockerBase() = default;

  PermissionDecisionAutoBlockerBase(const PermissionDecisionAutoBlockerBase&) =
      delete;
  PermissionDecisionAutoBlockerBase& operator=(
      const PermissionDecisionAutoBlockerBase&) = delete;

  // Returns whether |request_origin| is under embargo for |permission|.
  virtual bool IsEmbargoed(const GURL& request_origin,
                           ContentSettingsType permission) = 0;

  // Records that a dismissal of a prompt for |permission| was made. If the
  // total number of dismissals exceeds a threshhold and
  // features::kBlockPromptsIfDismissedOften is enabled, it will place |url|
  // under embargo for |permission|. |dismissed_prompt_was_quiet| will inform
  // the decision of which threshold to pick, depending on whether the prompt
  // that was presented to the user was quiet or not.
  virtual bool RecordDismissAndEmbargo(const GURL& url,
                                       ContentSettingsType permission,
                                       bool dismissed_prompt_was_quiet) = 0;

  // Records that an ignore of a prompt for |permission| was made. If the
  // total number of ignores exceeds a threshold and
  // features::kBlockPromptsIfIgnoredOften is enabled, it will place |url|
  // under embargo for |permission|. |ignored_prompt_was_quiet| will inform
  // the decision of which threshold to pick, depending on whether the prompt
  // that was presented to the user was quiet or not.
  virtual bool RecordIgnoreAndEmbargo(const GURL& url,
                                      ContentSettingsType permission,
                                      bool ignored_prompt_was_quiet) = 0;
};

// The PermissionDecisionAutoBlocker decides whether or not a given origin
// should be automatically blocked from requesting a permission. When an origin
// is blocked, it is placed under an "embargo". Until the embargo expires, any
// requests made by the origin are automatically blocked. Once the embargo is
// lifted, the origin will be permitted to request a permission again, which may
// result in it being placed under embargo again. Currently, an origin can be
// placed under embargo if it has a number of prior dismissals greater than a
// threshold.
class PermissionDecisionAutoBlocker : public PermissionDecisionAutoBlockerBase,
                                      public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEmbargoStarted(const GURL& origin,
                                  ContentSettingsType content_setting) = 0;
  };

  PermissionDecisionAutoBlocker() = delete;

  explicit PermissionDecisionAutoBlocker(HostContentSettingsMap* settings_map);

  ~PermissionDecisionAutoBlocker() override;

  // PermissionDecisionAutoBlockerBase
  bool IsEmbargoed(const GURL& request_origin,
                   ContentSettingsType permission) override;
  bool RecordDismissAndEmbargo(const GURL& url,
                               ContentSettingsType permission,
                               bool dismissed_prompt_was_quiet) override;
  bool RecordIgnoreAndEmbargo(const GURL& url,
                              ContentSettingsType permission,
                              bool ignored_prompt_was_quiet) override;

  // Returns whether the permission auto blocker is enabled for the passed-in
  // content setting.
  static bool IsEnabledForContentSetting(ContentSettingsType content_setting);

  // Checks the status of the content setting to determine if |request_origin|
  // is under embargo for |permission|. This checks all types of embargo.
  // Prefer to use PermissionManager::GetPermissionStatus when possible. This
  // method is only exposed to facilitate permission checks from threads other
  // than the UI thread. See https://crbug.com/658020.
  static std::optional<content::PermissionResult> GetEmbargoResult(
      HostContentSettingsMap* settings_map,
      const GURL& request_origin,
      ContentSettingsType permission,
      base::Time current_time);

  // Updates the threshold to start blocking prompts from the field trial.
  static void UpdateFromVariations();

  // Checks the status of the content setting to determine if |request_origin|
  // is under embargo for |permission|. This checks all types of embargo.
  std::optional<content::PermissionResult> GetEmbargoResult(
      const GURL& request_origin,
      ContentSettingsType permission);

  // Returns the most recent recorded time either an ignore or dismiss embargo
  // was started. Records of embargo start times persist beyond the duration
  // of the embargo, but are removed along with embargoes when
  // RemoveEmbargoAndResetCounts is used. Returns base::Time() if no record is
  // found.
  base::Time GetEmbargoStartTime(const GURL& request_origin,
                                 ContentSettingsType permission);

  // Returns the current number of dismisses recorded for |permission| type at
  // |url|.
  int GetDismissCount(const GURL& url, ContentSettingsType permission);

  // Returns the current number of ignores recorded for |permission|
  // type at |url|.
  int GetIgnoreCount(const GURL& url, ContentSettingsType permission);

  // Returns a set of urls currently under embargo for |content_type|.
  std::set<GURL> GetEmbargoedOrigins(ContentSettingsType content_type);

  // Returns a set of urls currently under embargo for the provided
  // |content_type| types.
  std::set<GURL> GetEmbargoedOrigins(
      std::vector<ContentSettingsType> content_types);

  // Records that a prompt was displayed for |permission|. If
  // features::kBlockRepeatedAutoReauthnPrompts is enabled, it will place |url|
  // under embargo for |permission|.
  bool RecordDisplayAndEmbargo(const GURL& url, ContentSettingsType permission);

  // Clears any existing embargo status for |url|, |permission|. For
  // permissions embargoed under repeated dismissals, this means a prompt will
  // be shown to the user on next permission request. Clears dismiss and
  // ignore counts.
  void RemoveEmbargoAndResetCounts(const GURL& url,
                                   ContentSettingsType permission);

  // Same as above, but cleans the slate for all permissions and for all URLs
  // matching |filter|.
  void RemoveEmbargoAndResetCounts(
      base::RepeatingCallback<bool(const GURL& url)> filter);

  // Add and remove observers that want to receive embargo status updates.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  static const char* GetPromptDismissCountKeyForTesting();

  void SetClockForTesting(base::Clock* clock);

 private:
  void PlaceUnderEmbargo(const GURL& request_origin,
                         ContentSettingsType permission,
                         const char* key);

  void NotifyEmbargoStarted(const GURL& origin,
                            ContentSettingsType content_setting);

  // Keys used for storing count data in a website setting.
  static const char kPromptDismissCountKey[];
  static const char kPromptIgnoreCountKey[];
  static const char kPromptDismissCountWithQuietUiKey[];
  static const char kPromptIgnoreCountWithQuietUiKey[];
  static const char kPermissionDismissalEmbargoKey[];
  static const char kPermissionIgnoreEmbargoKey[];
  static const char kPermissionDisplayEmbargoKey[];

  raw_ptr<HostContentSettingsMap> settings_map_;

  raw_ptr<base::Clock> clock_;

  base::ObserverList<Observer> observers_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
