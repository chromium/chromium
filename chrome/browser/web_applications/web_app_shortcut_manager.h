// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SHORTCUT_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace web_app {

class FileHandlerManager;
class ProtocolHandlerManager;
class WebApp;
class WebAppIconManager;
class WebAppRegistrar;
struct ShortcutInfo;

class WebAppShortcutManager : public AppShortcutManager {
 public:
  WebAppShortcutManager(Profile* profile,
                        WebAppIconManager* icon_manager,
                        FileHandlerManager* file_handler_manager,
                        ProtocolHandlerManager* protocol_handler_manager);
  WebAppShortcutManager(const WebAppShortcutManager&) = delete;
  WebAppShortcutManager& operator=(const WebAppShortcutManager&) = delete;
  ~WebAppShortcutManager() override;

  // AppShortcutManager:
  std::unique_ptr<ShortcutInfo> BuildShortcutInfo(const AppId& app_id) override;
  void GetShortcutInfoForApp(const AppId& app_id,
                             GetShortcutInfoCallback callback) override;

 private:
  void OnIconsRead(const AppId& app_id,
                   GetShortcutInfoCallback callback,
                   std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  std::unique_ptr<ShortcutInfo> BuildShortcutInfoForWebApp(const WebApp* app);

  WebAppRegistrar& GetWebAppRegistrar();

  WebAppIconManager* icon_manager_;
  FileHandlerManager* file_handler_manager_;
  ProtocolHandlerManager* protocol_handler_manager_;

  base::WeakPtrFactory<WebAppShortcutManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SHORTCUT_MANAGER_H_
