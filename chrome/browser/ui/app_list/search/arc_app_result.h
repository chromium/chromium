// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_APP_RESULT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "components/arc/metrics/arc_metrics_constants.h"

class AppListControllerDelegate;
class ArcAppContextMenu;
class ArcAppIconLoader;
class Profile;

namespace app_list {

class ArcAppResult : public AppResult,
                     public AppIconLoaderDelegate {
 public:
  ArcAppResult(Profile* profile,
               const std::string& app_id,
               AppListControllerDelegate* controller,
               bool is_recommendation);
  ~ArcAppResult() override;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  void GetContextMenuModel(GetMenuModelCallback callback) override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  // AppIconLoaderDelegate overrides:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

 private:
  // ChromeSearchResult overrides:
  AppContextMenu* GetAppContextMenu() override;

  void Launch(int event_flags, arc::UserInteractionType interaction);
  arc::UserInteractionType GetAppLaunchInteraction();
  arc::UserInteractionType GetContextMenuAppLaunchInteraction();

  std::unique_ptr<ArcAppIconLoader> icon_loader_;
  std::unique_ptr<ArcAppIconLoader> chip_icon_loader_;
  std::unique_ptr<ArcAppContextMenu> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_APP_RESULT_H_
