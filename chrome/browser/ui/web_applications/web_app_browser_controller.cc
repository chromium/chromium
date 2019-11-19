// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace web_app {

WebAppBrowserController::WebAppBrowserController(Browser* browser)
    : AppBrowserController(browser,
                           GetAppIdFromApplicationName(browser->app_name())),
      provider_(*WebAppProvider::Get(browser->profile())) {}

WebAppBrowserController::~WebAppBrowserController() = default;

bool WebAppBrowserController::CreatedForInstalledPwa() const {
  return !registrar().IsShortcutApp(GetAppId());
}

bool WebAppBrowserController::HasMinimalUiButtons() const {
  return registrar().GetAppEffectiveDisplayMode(GetAppId()) ==
         DisplayMode::kMinimalUi;
}

bool WebAppBrowserController::IsHostedApp() const {
  return true;
}

void WebAppBrowserController::SetReadIconCallbackForTesting(
    base::OnceClosure callback) {
  callback_for_testing_ = std::move(callback);
}

gfx::ImageSkia WebAppBrowserController::GetWindowAppIcon() const {
  if (app_icon_)
    return *app_icon_;
  app_icon_ = GetFallbackAppIcon();

  provider_.icon_manager().ReadSmallestIcon(
      GetAppId(), gfx::kFaviconSize,
      base::BindOnce(&WebAppBrowserController::OnReadIcon,
                     weak_ptr_factory_.GetWeakPtr()));

  return *app_icon_;
}

gfx::ImageSkia WebAppBrowserController::GetWindowIcon() const {
  return GetWindowAppIcon();
}

base::Optional<SkColor> WebAppBrowserController::GetThemeColor() const {
  base::Optional<SkColor> web_theme_color =
      AppBrowserController::GetThemeColor();
  if (web_theme_color)
    return web_theme_color;

  return registrar().GetAppThemeColor(GetAppId());
}

GURL WebAppBrowserController::GetAppLaunchURL() const {
  return registrar().GetAppLaunchURL(GetAppId());
}

bool WebAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  base::Optional<GURL> app_scope = registrar().GetAppScope(GetAppId());
  if (!app_scope)
    return false;

  // https://w3c.github.io/manifest/#navigation-scope
  // If url is same origin as scope and url path starts with scope path, return
  // true. Otherwise, return false.
  if (app_scope->GetOrigin() != url.GetOrigin())
    return false;

  std::string scope_path = app_scope->path();
  std::string url_path = url.path();
  return base::StartsWith(url_path, scope_path, base::CompareCase::SENSITIVE);
}

WebAppBrowserController* WebAppBrowserController::AsWebAppBrowserController() {
  return this;
}

std::string WebAppBrowserController::GetAppShortName() const {
  return registrar().GetAppShortName(GetAppId());
}

base::string16 WebAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppLaunchURL());
}

bool WebAppBrowserController::CanUninstall() const {
  return WebAppUiManagerImpl::Get(browser()->profile())
      ->dialog_manager()
      .CanUninstallWebApp(GetAppId());
}

void WebAppBrowserController::Uninstall() {
  WebAppUiManagerImpl::Get(browser()->profile())
      ->dialog_manager()
      .UninstallWebApp(GetAppId(),
                       WebAppDialogManager::UninstallSource::kAppMenu,
                       browser()->window(), base::DoNothing());
}

bool WebAppBrowserController::IsInstalled() const {
  return registrar().IsInstalled(GetAppId());
}

const AppRegistrar& WebAppBrowserController::registrar() const {
  return provider_.registrar();
}

void WebAppBrowserController::OnReadIcon(SkBitmap bitmap) {
  if (bitmap.empty()) {
    DLOG(ERROR) << "Failed to read icon for web app";
    return;
  }

  app_icon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  if (callback_for_testing_)
    std::move(callback_for_testing_).Run();
}

}  // namespace web_app
