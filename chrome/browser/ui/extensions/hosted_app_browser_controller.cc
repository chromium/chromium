// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Returns true if |app_url| and |page_url| are the same origin. To avoid
// breaking Hosted Apps and Bookmark Apps that might redirect to sites in the
// same domain but with "www.", this returns true if |page_url| is secure and in
// the same origin as |app_url| with "www.".
bool IsSameHostAndPort(const GURL& app_url, const GURL& page_url) {
  return (app_url.host_piece() == page_url.host_piece() ||
          std::string("www.") + app_url.host() == page_url.host_piece()) &&
         app_url.port() == page_url.port();
}

}  // namespace

HostedAppBrowserController::HostedAppBrowserController(Browser* browser)
    : AppBrowserController(
          browser,
          web_app::GetAppIdFromApplicationName(browser->app_name())) {}

HostedAppBrowserController::~HostedAppBrowserController() = default;

bool HostedAppBrowserController::HasMinimalUiButtons() const {
  return false;
}

ui::ImageModel HostedAppBrowserController::GetWindowAppIcon() const {
  // TODO(calamity): Use the app name to retrieve the app icon without using the
  // extensions tab helper to make icon load more immediate.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          browser()->profile())) {
    if (!app_icon_.isNull())
      return ui::ImageModel::FromImageSkia(app_icon_);

    const Extension* extension = GetExtension();
    if (extension &&
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
                ->AppRegistryCache()
                .GetAppType(extension->id()) != apps::AppType::kUnknown) {
      LoadAppIcon(true /* allow_placeholder_icon */);
      return GetFallbackAppIcon();
    }
  }
#endif

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return GetFallbackAppIcon();

  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  if (!extensions_tab_helper)
    return GetFallbackAppIcon();

  const SkBitmap* icon_bitmap = extensions_tab_helper->GetExtensionAppIcon();
  if (!icon_bitmap)
    return GetFallbackAppIcon();

  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(*icon_bitmap));
}

ui::ImageModel HostedAppBrowserController::GetWindowIcon() const {
  if (IsWebApp(browser()))
    return GetWindowAppIcon();

  return ui::ImageModel::FromImage(browser()->GetCurrentPageIcon());
}

std::u16string HostedAppBrowserController::GetTitle() const {
  // When showing the toolbar, display the name of the app, instead of the
  // current page as the title.
  if (ShouldShowCustomTabBar()) {
    const Extension* extension = GetExtension();
    return base::UTF8ToUTF16(extension->name());
  }

  return AppBrowserController::GetTitle();
}

GURL HostedAppBrowserController::GetAppStartUrl() const {
  const Extension* extension = GetExtension();
  if (!extension)
    return GURL();

  return AppLaunchInfo::GetLaunchWebURL(extension);
}

bool HostedAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  const Extension* extension = GetExtension();

  if (!extension)
    return false;

  const std::vector<UrlHandlerInfo>* url_handlers =
      UrlHandlers::GetUrlHandlers(extension);

  // We don't have a scope, fall back to same origin check.
  if (!url_handlers)
    return IsSameHostAndPort(GetAppStartUrl(), url);

  return UrlHandlers::CanBookmarkAppHandleUrl(extension, url);
}

const Extension* HostedAppBrowserController::GetExtension() const {
  return ExtensionRegistry::Get(browser()->profile())
      ->GetExtensionById(app_id(), ExtensionRegistry::EVERYTHING);
}

std::u16string HostedAppBrowserController::GetAppShortName() const {
  const Extension* extension = GetExtension();
  return extension ? base::UTF8ToUTF16(extension->short_name())
                   : std::u16string();
}

std::u16string HostedAppBrowserController::GetFormattedUrlOrigin() const {
  const Extension* extension = GetExtension();
  return extension ? FormatUrlOrigin(AppLaunchInfo::GetLaunchWebURL(extension))
                   : std::u16string();
}

bool HostedAppBrowserController::CanUserUninstall() const {
  if (uninstall_dialog_)
    return false;

  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  return extensions::ExtensionSystem::Get(browser()->profile())
      ->management_policy()
      ->UserMayModifySettings(extension, nullptr);
}

void HostedAppBrowserController::Uninstall(
    webapps::WebappUninstallSource webapp_uninstall_source) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  DCHECK(!uninstall_dialog_);
  uninstall_dialog_ = ExtensionUninstallDialog::Create(
      browser()->profile(),
      browser()->window() ? browser()->window()->GetNativeWindow() : nullptr,
      this);

  // The dialog can be closed by UI system whenever it likes, but
  // OnExtensionUninstallDialogClosed will be called anyway.
  uninstall_dialog_->ConfirmUninstall(extension,
                                      UNINSTALL_REASON_USER_INITIATED,
                                      UNINSTALL_SOURCE_HOSTED_APP_MENU);
}

bool HostedAppBrowserController::IsInstalled() const {
  return GetExtension();
}

bool HostedAppBrowserController::IsHostedApp() const {
  return true;
}

void HostedAppBrowserController::OnExtensionUninstallDialogClosed(
    bool success,
    const std::u16string& error) {
  uninstall_dialog_.reset();
}

void HostedAppBrowserController::OnTabInserted(content::WebContents* contents) {
  AppBrowserController::OnTabInserted(contents);

  const Extension* extension = GetExtension();
  extensions::TabHelper::FromWebContents(contents)->SetExtensionApp(extension);
}

void HostedAppBrowserController::OnTabRemoved(content::WebContents* contents) {
  AppBrowserController::OnTabRemoved(contents);

  extensions::TabHelper::FromWebContents(contents)->SetExtensionApp(nullptr);
}

void HostedAppBrowserController::LoadAppIcon(
    bool allow_placeholder_icon) const {
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->LoadIcon(GetExtension()->id(), apps::IconType::kStandard,
                 extension_misc::EXTENSION_ICON_SMALL, allow_placeholder_icon,
                 base::BindOnce(&HostedAppBrowserController::OnLoadIcon,
                                weak_ptr_factory_.GetMutableWeakPtr()));
}

void HostedAppBrowserController::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  app_icon_ = icon_value->uncompressed;

  if (icon_value->is_placeholder_icon)
    LoadAppIcon(false /* allow_placeholder_icon */);
}

}  // namespace extensions
