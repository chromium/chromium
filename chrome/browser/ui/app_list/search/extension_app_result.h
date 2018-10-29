// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_EXTENSION_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_EXTENSION_APP_RESULT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry_observer.h"

class AppListControllerDelegate;
class ExtensionEnableFlow;
class Profile;

namespace extensions {
class ExtensionRegistry;
}

namespace app_list {

class ExtensionAppContextMenu;

class ExtensionAppResult : public AppResult,
                           public extensions::ChromeAppIconDelegate,
                           public ExtensionEnableFlowDelegate,
                           public extensions::ExtensionRegistryObserver {
 public:
  ExtensionAppResult(Profile* profile,
                     const std::string& app_id,
                     AppListControllerDelegate* controller,
                     bool is_recommendation);
  ~ExtensionAppResult() override;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  void GetContextMenuModel(GetMenuModelCallback callback) override;

 private:
  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  // Checks if extension is disabled and if enable flow should be started.
  // Returns true if extension enable flow is started or there is already one
  // running.
  bool RunExtensionEnableFlow();

  // ChromeSearchResult overrides:
  AppContextMenu* GetAppContextMenu() override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  // ExtensionEnableFlowDelegate overrides:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  // extensions::ExtensionRegistryObserver override:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // extensions::ChromeAppIconDelegate:
  void OnIconUpdated(extensions::ChromeAppIcon* icon) override;

  bool is_platform_app_;
  std::unique_ptr<extensions::ChromeAppIcon> icon_;
  std::unique_ptr<extensions::ChromeAppIcon> chip_icon_;
  std::unique_ptr<ExtensionAppContextMenu> context_menu_;
  std::unique_ptr<ExtensionEnableFlow> extension_enable_flow_;

  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_EXTENSION_APP_RESULT_H_
