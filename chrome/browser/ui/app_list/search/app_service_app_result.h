// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/services/app_service/public/cpp/icon_cache.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
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

class AppServiceAppResult : public AppResult {
 public:
  AppServiceAppResult(Profile* profile,
                      const std::string& app_id,
                      AppListControllerDelegate* controller,
                      bool is_recommendation,
                      apps::IconLoader* icon_loader);
  ~AppServiceAppResult() override;

 private:
  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  void GetContextMenuModel(GetMenuModelCallback callback) override;
  void OnVisibilityChanged(bool visibility) override;
  ash::SearchResultType GetSearchResultType() const override;
  AppContextMenu* GetAppContextMenu() override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  void Launch(int event_flags, apps::mojom::LaunchSource launch_source);

  void CallLoadIcon(bool chip, bool allow_placeholder_icon);
  void OnLoadIcon(bool chip, apps::mojom::IconValuePtr icon_value);

  void HandleSuggestionChip(Profile* profile);

  // Get large icon image from servers and update icon for continue reading.
  //
  // Continue reading is a special app, which uses LargeIconService to load
  // icon. Since it is the only app which uses the LargeIconService, leave it in
  // search result code, not integrated to the AppService icon cache.
  //
  // If there is no cache hit on LargeIconService and
  // |continue_to_google_server| is true, will try to download the icon from
  // Google favicon server.
  void UpdateContinueReadingFavicon(bool continue_to_google_server);
  void OnGetFaviconFromCacheFinished(
      bool continue_to_google_server,
      const favicon_base::LargeIconImageResult& image);
  void OnGetFaviconFromGoogleServerFinished(
      favicon_base::GoogleFaviconServerRequestStatus status);

  apps::IconLoader* const icon_loader_;

  // When non-nullptr, signifies that this object is using the most recent icon
  // fetched from |icon_loader_|. When destroyed, informs |icon_loader_| that
  // the last icon is no longer used.
  std::unique_ptr<apps::IconLoader::Releaser> icon_loader_releaser_;

  apps::mojom::AppType app_type_;
  bool is_platform_app_;
  bool show_in_launcher_;

  std::unique_ptr<AppContextMenu> context_menu_;

  base::CancelableTaskTracker task_tracker_;

  // The url of recommendable foreign tab, which is invalid if there is no
  // recommendation.
  GURL url_for_continuous_reading_;

  // Used to fetch the favicon of the website |url_for_continuous_reading_|.
  favicon::LargeIconService* large_icon_service_ = nullptr;

  base::WeakPtrFactory<AppServiceAppResult> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
