// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_INTERNAL_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_INTERNAL_APP_RESULT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "components/favicon_base/favicon_types.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace favicon {
class LargeIconService;
}  // namespace favicon

namespace favicon_base {
struct LargeIconImageResult;
}  // namespace favicon_base

namespace app_list {

class AppContextMenu;

class InternalAppResult : public AppResult {
 public:
  InternalAppResult(Profile* profile,
                    const std::string& app_id,
                    AppListControllerDelegate* controller,
                    bool is_recommendation);
  ~InternalAppResult() override;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  void GetContextMenuModel(GetMenuModelCallback callback) override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

 private:
  // ChromeSearchResult overrides:
  AppContextMenu* GetAppContextMenu() override;

  // Get large icon image from servers and update icon for continue reading.
  // If there is no cache hit on LargeIconService and
  // |continue_to_google_server| is true, will try to download the icon from
  // Google favicon server.
  void UpdateContinueReadingFavicon(bool continue_to_google_server);
  void OnGetFaviconFromCacheFinished(
      bool continue_to_google_server,
      const favicon_base::LargeIconImageResult& image);
  void OnGetFaviconFromGoogleServerFinished(
      favicon_base::GoogleFaviconServerRequestStatus status);

  std::unique_ptr<AppContextMenu> context_menu_;

  base::CancelableTaskTracker task_tracker_;

  // The url of recommendable foreign tab, which is invalid if there is no
  // recommendation.
  GURL url_for_continuous_reading_;

  // Used to fetch the favicon of the website |url_for_continuous_reading_|.
  favicon::LargeIconService* large_icon_service_ = nullptr;

  base::WeakPtrFactory<InternalAppResult> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InternalAppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_INTERNAL_APP_RESULT_H_
