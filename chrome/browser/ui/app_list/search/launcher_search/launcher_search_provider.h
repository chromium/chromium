// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_PROVIDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "extensions/common/extension_id.h"

namespace app_list {

// LauncherSearchProvider dispatches queries to extensions and fetches the
// results from them via chrome.launcherSearchProvider API.
class LauncherSearchProvider : public SearchProvider {
 public:
  explicit LauncherSearchProvider(Profile* profile);
  ~LauncherSearchProvider() override;

  void Start(const base::string16& query) override;
  void SetSearchResults(
      const extensions::ExtensionId& extension_id,
      std::vector<std::unique_ptr<LauncherSearchResult>> extension_results);

 private:
  // Delays query for |kLauncherSearchProviderQueryDelayInMs|. This dispatches
  // the latest query after no more calls to Start() for the delay duration.
  void DelayQuery(const base::Closure& closure);

  // Dispatches |query| to LauncherSearchProvider service.
  void StartInternal(const base::string16& query);

  // The search results of each extension.
  std::map<extensions::ExtensionId,
           std::vector<std::unique_ptr<LauncherSearchResult>>>
      extension_results_;

  // A timer to delay query.
  base::OneShotTimer query_timer_;

  // The timestamp of the last query.
  base::Time last_query_time_;

  base::TimeTicks query_start_time_;

  // The reference to profile to get LauncherSearchProvider service.
  Profile* profile_;

  base::WeakPtrFactory<LauncherSearchProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LauncherSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_PROVIDER_H_
