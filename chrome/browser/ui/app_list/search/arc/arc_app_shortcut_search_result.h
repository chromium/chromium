// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUT_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUT_SEARCH_RESULT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/arc/mojom/app.mojom.h"
#include "ui/gfx/image/image_skia.h"

class AppListControllerDelegate;
class Profile;

namespace arc {
class IconDecodeRequest;
}  // namespace arc

namespace app_list {

class ArcAppShortcutSearchResult : public ChromeSearchResult,
                                   public AppIconLoaderDelegate {
 public:
  // Constructor for ArcAppShortcutSearchResult. |is_recommendation|
  // defines the display type of search results.
  ArcAppShortcutSearchResult(arc::mojom::AppShortcutItemPtr data,
                             Profile* profile,
                             AppListControllerDelegate* list_controller,
                             bool is_recommendation);
  ~ArcAppShortcutSearchResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  ash::SearchResultType GetSearchResultType() const override;

 private:
  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

  // Gets app id of the app that publishes this app shortcut.
  std::string GetAppId() const;

  // Gets accessible name for this app shortcut.
  base::string16 ComputeAccessibleName() const;

  arc::mojom::AppShortcutItemPtr data_;
  std::unique_ptr<arc::IconDecodeRequest> icon_decode_request_;

  std::unique_ptr<ArcAppIconLoader> badge_icon_loader_;

  Profile* const profile_;                            // Owned by ProfileInfo.
  AppListControllerDelegate* const list_controller_;  // Owned by AppListClient.

  DISALLOW_COPY_AND_ASSIGN(ArcAppShortcutSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUT_SEARCH_RESULT_H_
