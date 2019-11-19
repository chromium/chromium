// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_MANAGER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class ListValue;
class SequencedTaskRunner;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

// Contains a set of filters to block and allow certain URLs, and matches GURLs
// against this set. The filters are currently kept in memory.
class POLICY_EXPORT URLBlacklist {
 public:
  // Indicates if the URL matches a pattern defined in blacklist, in whitelist
  // or doesn't match anything in either list as defined in URLBlacklist and
  // URLWhitelist policies.
  enum URLBlacklistState {
    URL_IN_WHITELIST,
    URL_IN_BLACKLIST,
    URL_NEUTRAL_STATE,
  };

  URLBlacklist();
  virtual ~URLBlacklist();

  // URLs matching one of the |filters| will be blocked. The filter format is
  // documented at
  // http://www.chromium.org/administrators/url-blacklist-filter-format.
  void Block(const base::ListValue* filters);

  // URLs matching one of the |filters| will be allowed. If a URL is both
  // Blocked and Allowed, Allow takes precedence.
  void Allow(const base::ListValue* filters);

  // Returns true if the URL is blocked.
  bool IsURLBlocked(const GURL& url) const;

  URLBlacklistState GetURLBlacklistState(const GURL& url) const;

  // Returns the number of items in the list.
  size_t Size() const;

 private:

  // Returns true if |lhs| takes precedence over |rhs|.
  static bool FilterTakesPrecedence(const url_util::FilterComponents& lhs,
                                    const url_util::FilterComponents& rhs);

  url_matcher::URLMatcherConditionSet::ID id_;
  std::map<url_matcher::URLMatcherConditionSet::ID, url_util::FilterComponents>
      filters_;
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklist);
};

// Tracks the blacklist policies for a given profile, and updates it on changes.
class POLICY_EXPORT URLBlacklistManager {
 public:
  // Must be constructed on the UI thread.
  explicit URLBlacklistManager(PrefService* pref_service);
  virtual ~URLBlacklistManager();

  // Returns true if |url| is blocked by the current blacklist.
  bool IsURLBlocked(const GURL& url) const;

  URLBlacklist::URLBlacklistState GetURLBlacklistState(const GURL& url) const;

  // Replaces the current blacklist.
  // Virtual for testing.
  virtual void SetBlacklist(std::unique_ptr<URLBlacklist> blacklist);

  // Registers the preferences related to blacklisting in the given PrefService.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  // Used to delay updating the blacklist while the preferences are
  // changing, and execute only one update per simultaneous prefs changes.
  void ScheduleUpdate();

  // Updates the blacklist using the current preference values.
  // Virtual for testing.
  virtual void Update();

 private:
  // Used to track the policies and update the blacklist on changes.
  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;  // Weak.

  // Used to post tasks to a background thread.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Used to schedule tasks on the main loop to avoid rebuilding the blocklist
  // multiple times during a message loop process. This can happen if two
  // preferences that change the blacklist are updated in one message loop
  // cycle.  In addition, we use this task runner to ensure that the
  // URLBlocklistManager is only access from the thread call the constructor for
  // data accesses.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // The current blacklist.
  std::unique_ptr<URLBlacklist> blacklist_;

  // Used to post update tasks to the UI thread.
  base::WeakPtrFactory<URLBlacklistManager> ui_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLBlacklistManager);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_MANAGER_H_
