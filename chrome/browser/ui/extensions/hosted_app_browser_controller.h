// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class Browser;

namespace gfx {
class ImageSkia;
}

namespace extensions {

class Extension;

// Class to encapsulate logic to control the browser UI for extension based
// Chrome Apps (platform apps and legacy packaged apps).
class HostedAppBrowserController : public web_app::AppBrowserController,
                                   public ExtensionUninstallDialog::Delegate {
 public:
  explicit HostedAppBrowserController(Browser* browser);

  HostedAppBrowserController(const HostedAppBrowserController&) = delete;
  HostedAppBrowserController& operator=(const HostedAppBrowserController&) =
      delete;

  ~HostedAppBrowserController() override;

  // web_app::AppBrowserController:
  bool HasMinimalUiButtons() const override;
  ui::ImageModel GetWindowAppIcon() const override;
  ui::ImageModel GetWindowIcon() const override;
  std::u16string GetTitle() const override;
  std::u16string GetAppShortName() const override;
  std::u16string GetFormattedUrlOrigin() const override;
  GURL GetAppStartUrl() const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  bool CanUserUninstall() const override;
  void Uninstall(
      webapps::WebappUninstallSource webapp_uninstall_source) override;
  bool IsInstalled() const override;
  bool IsHostedApp() const override;

 protected:
  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool success,
                                        const std::u16string& error) override;

  // web_app::AppBrowserController:
  void OnTabInserted(content::WebContents* contents) override;
  void OnTabRemoved(content::WebContents* contents) override;

 private:
  // Will return nullptr if the extension has been uninstalled.
  const Extension* GetExtension() const;

  // Helper function to call AppServiceProxy to load icon.
  void LoadAppIcon(bool allow_placeholder_icon) const;
  // Invoked when the icon is loaded.
  void OnLoadIcon(apps::IconValuePtr icon_value);

  gfx::ImageSkia app_icon_;

  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  base::WeakPtrFactory<HostedAppBrowserController> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
