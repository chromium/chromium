// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/apps/apk_web_app_service.h"

namespace {
constexpr char kRelationship[] = "delegate_permission/common.handle_all_urls";
}
#endif

namespace {

// SystemWebAppDelegate provides menu.
class SystemAppTabMenuModelFactory : public TabMenuModelFactory {
 public:
  explicit SystemAppTabMenuModelFactory(
      const web_app::SystemWebAppDelegate* system_app)
      : system_app_(system_app) {}
  SystemAppTabMenuModelFactory(const SystemAppTabMenuModelFactory&) = delete;
  SystemAppTabMenuModelFactory& operator=(const SystemAppTabMenuModelFactory&) =
      delete;
  ~SystemAppTabMenuModelFactory() override = default;

  std::unique_ptr<ui::SimpleMenuModel> Create(
      ui::SimpleMenuModel::Delegate* delegate,
      TabMenuModelDelegate* tab_menu_model_delegate,
      TabStripModel*,
      int) override {
    return system_app_->GetTabMenuModel(delegate);
  }

 private:
  raw_ptr<const web_app::SystemWebAppDelegate> system_app_;
};

}  // namespace

namespace web_app {

WebAppBrowserController::WebAppBrowserController(
    WebAppProvider& provider,
    Browser* browser,
    AppId app_id,
    const SystemWebAppDelegate* system_app,
    bool has_tab_strip)
    : AppBrowserController(browser, std::move(app_id), has_tab_strip),
      provider_(provider),
      system_app_(system_app) {
  install_manager_observation_.Observe(&provider.install_manager());
  PerformDigitalAssetLinkVerification(browser);
}

WebAppBrowserController::~WebAppBrowserController() = default;

bool WebAppBrowserController::HasMinimalUiButtons() const {
  if (has_tab_strip())
    return false;
  DisplayMode app_display_mode =
      registrar().GetEffectiveDisplayModeFromManifest(app_id());
  return app_display_mode == DisplayMode::kBrowser ||
         app_display_mode == DisplayMode::kMinimalUi;
}

bool WebAppBrowserController::IsHostedApp() const {
  return true;
}

std::unique_ptr<TabMenuModelFactory>
WebAppBrowserController::GetTabMenuModelFactory() const {
  if (system_app() && system_app()->HasCustomTabMenuModel()) {
    return std::make_unique<SystemAppTabMenuModelFactory>(system_app());
  }
  return nullptr;
}

bool WebAppBrowserController::AppUsesWindowControlsOverlay() const {
  DisplayMode display = registrar().GetAppEffectiveDisplayMode(app_id());
  return display == DisplayMode::kWindowControlsOverlay;
}

bool WebAppBrowserController::IsWindowControlsOverlayEnabled() const {
  return AppUsesWindowControlsOverlay() &&
         registrar().GetWindowControlsOverlayEnabled(app_id());
}

void WebAppBrowserController::ToggleWindowControlsOverlayEnabled() {
  DCHECK(AppUsesWindowControlsOverlay());

  provider_.sync_bridge().SetAppWindowControlsOverlayEnabled(
      app_id(), !registrar().GetWindowControlsOverlayEnabled(app_id()));
}

gfx::Rect WebAppBrowserController::GetDefaultBounds() const {
  if (system_app_) {
    return system_app_->GetDefaultBounds(browser());
  }

  return gfx::Rect();
}

bool WebAppBrowserController::HasReloadButton() const {
  if (!system_app_)
    return true;

  return system_app_->ShouldHaveReloadButtonInMinimalUi();
}

const SystemWebAppDelegate* WebAppBrowserController::system_app() const {
  return system_app_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool WebAppBrowserController::ShouldShowCustomTabBar() const {
  if (AppBrowserController::ShouldShowCustomTabBar())
    return true;

  return is_verified_.value_or(false);
}

void WebAppBrowserController::OnRelationshipCheckComplete(
    digital_asset_links::RelationshipCheckResult result) {
  bool should_show_cct = false;
  switch (result) {
    case digital_asset_links::RelationshipCheckResult::kSuccess:
      should_show_cct = false;
      break;
    case digital_asset_links::RelationshipCheckResult::kFailure:
    case digital_asset_links::RelationshipCheckResult::kNoConnection:
      should_show_cct = true;
      break;
  }
  is_verified_ = should_show_cct;
  browser()->window()->UpdateCustomTabBarVisibility(should_show_cct,
                                                    false /* animate */);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void WebAppBrowserController::OnWebAppUninstalled(
    const AppId& uninstalled_app_id) {
  if (uninstalled_app_id == app_id())
    chrome::CloseWindow(browser());
}

void WebAppBrowserController::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void WebAppBrowserController::SetReadIconCallbackForTesting(
    base::OnceClosure callback) {
  callback_for_testing_ = std::move(callback);
}

ui::ImageModel WebAppBrowserController::GetWindowAppIcon() const {
  if (app_icon_)
    return *app_icon_;
  app_icon_ = GetFallbackAppIcon();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          browser()->profile())) {
    LoadAppIcon(true /* allow_placeholder_icon */);
    return *app_icon_;
  }
#endif

  if (provider_.icon_manager().HasSmallestIcon(app_id(), {IconPurpose::ANY},
                                               kWebAppIconSmall)) {
    provider_.icon_manager().ReadSmallestIconAny(
        app_id(), kWebAppIconSmall,
        base::BindOnce(&WebAppBrowserController::OnReadIcon,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  return *app_icon_;
}

ui::ImageModel WebAppBrowserController::GetWindowIcon() const {
  return GetWindowAppIcon();
}

absl::optional<SkColor> WebAppBrowserController::GetThemeColor() const {
  // System App popups (settings pages) always use default theme.
  if (system_app_ && browser()->is_type_app_popup())
    return absl::nullopt;

  absl::optional<SkColor> web_theme_color =
      AppBrowserController::GetThemeColor();
  if (web_theme_color)
    return web_theme_color;

  if (ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    absl::optional<SkColor> dark_mode_color =
        registrar().GetAppDarkModeThemeColor(app_id());

    if (dark_mode_color) {
      return dark_mode_color;
    }
  }

  return registrar().GetAppThemeColor(app_id());
}

absl::optional<SkColor> WebAppBrowserController::GetBackgroundColor() const {
  auto web_contents_color = AppBrowserController::GetBackgroundColor();
  auto manifest_color = GetResolvedManifestBackgroundColor();
  auto [preferred_color, fallback_color] =
      (system_app() && system_app()->PreferManifestBackgroundColor())
          ? std::tie(manifest_color, web_contents_color)
          : std::tie(web_contents_color, manifest_color);
  return preferred_color ? preferred_color : fallback_color;
}

GURL WebAppBrowserController::GetAppStartUrl() const {
  return registrar().GetAppStartUrl(app_id());
}

bool WebAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  if (system_app() && system_app()->IsUrlInSystemAppScope(url))
    return true;

  GURL app_scope = registrar().GetAppScope(app_id());
  if (!app_scope.is_valid())
    return false;

  // https://w3c.github.io/manifest/#navigation-scope
  // If url is same origin as scope and url path starts with scope path, return
  // true. Otherwise, return false.
  if (app_scope.DeprecatedGetOriginAsURL() != url.DeprecatedGetOriginAsURL()) {
    // We allow an upgrade from http |app_scope| to https |url|.
    if (app_scope.scheme() != url::kHttpScheme)
      return false;

    GURL::Replacements rep;
    rep.SetSchemeStr(url::kHttpsScheme);
    GURL secure_app_scope = app_scope.ReplaceComponents(rep);
    if (secure_app_scope.DeprecatedGetOriginAsURL() !=
        url.DeprecatedGetOriginAsURL())
      return false;
  }

  std::string scope_path = app_scope.path();
  std::string url_path = url.path();
  return base::StartsWith(url_path, scope_path, base::CompareCase::SENSITIVE);
}

WebAppBrowserController* WebAppBrowserController::AsWebAppBrowserController() {
  return this;
}

std::u16string WebAppBrowserController::GetTitle() const {
  // When showing the toolbar, display the name of the app, instead of the
  // current page as the title.
  if (ShouldShowCustomTabBar()) {
    // TODO(crbug.com/1051379): Use name instead of short_name.
    return base::UTF8ToUTF16(registrar().GetAppShortName(app_id()));
  }

  std::u16string raw_title = AppBrowserController::GetTitle();

  if (!base::FeatureList::IsEnabled(features::kPrefixWebAppWindowsWithAppName))
    return raw_title;

  std::u16string app_name =
      base::UTF8ToUTF16(provider_.registrar().GetAppShortName(app_id()));
  if (base::StartsWith(raw_title, app_name)) {
    return raw_title;
  }

  if (raw_title.empty()) {
    return app_name;
  }

  return base::StrCat({app_name, u" - ", raw_title});
}

std::u16string WebAppBrowserController::GetAppShortName() const {
  return base::UTF8ToUTF16(registrar().GetAppShortName(app_id()));
}

std::u16string WebAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppStartUrl());
}

bool WebAppBrowserController::CanUserUninstall() const {
  return WebAppUiManagerImpl::Get(&provider_)
      ->dialog_manager()
      .CanUserUninstallWebApp(app_id());
}

void WebAppBrowserController::Uninstall(
    webapps::WebappUninstallSource webapp_uninstall_source) {
  WebAppUiManagerImpl::Get(&provider_)
      ->dialog_manager()
      .UninstallWebApp(app_id(), webapps::WebappUninstallSource::kAppMenu,
                       browser()->window(), base::DoNothing());
}

bool WebAppBrowserController::IsInstalled() const {
  return registrar().IsInstalled(app_id());
}

void WebAppBrowserController::OnTabInserted(content::WebContents* contents) {
  AppBrowserController::OnTabInserted(contents);
  SetAppPrefsForWebContents(contents);

  // If a `WebContents` is inserted into an app browser (e.g. after
  // installation), it is "appy". Note that if and when it's moved back into a
  // tabbed browser window (e.g. via "Open in Chrome" menu item), it is still
  // considered "appy".
  WebAppTabHelper::FromWebContents(contents)->set_acting_as_app(true);
}

void WebAppBrowserController::OnTabRemoved(content::WebContents* contents) {
  AppBrowserController::OnTabRemoved(contents);
  ClearAppPrefsForWebContents(contents);
}

const WebAppRegistrar& WebAppBrowserController::registrar() const {
  return provider_.registrar();
}

const WebAppInstallManager& WebAppBrowserController::install_manager() const {
  return provider_.install_manager();
}

void WebAppBrowserController::LoadAppIcon(bool allow_placeholder_icon) const {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  auto app_type = proxy->AppRegistryCache().GetAppType(app_id());
  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    proxy->LoadIcon(app_type, app_id(), apps::IconType::kStandard,
                    kWebAppIconSmall, allow_placeholder_icon,
                    base::BindOnce(&WebAppBrowserController::OnLoadIcon,
                                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    proxy->LoadIcon(apps::ConvertAppTypeToMojomAppType(app_type), app_id(),
                    apps::mojom::IconType::kStandard, kWebAppIconSmall,
                    allow_placeholder_icon,
                    apps::MojomIconValueToIconValueCallback(
                        base::BindOnce(&WebAppBrowserController::OnLoadIcon,
                                       weak_ptr_factory_.GetWeakPtr())));
  }
}

void WebAppBrowserController::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  app_icon_ = ui::ImageModel::FromImageSkia(icon_value->uncompressed);

  if (icon_value->is_placeholder_icon)
    LoadAppIcon(false /* allow_placeholder_icon */);

  if (auto* contents = web_contents())
    contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  if (callback_for_testing_)
    std::move(callback_for_testing_).Run();
}

void WebAppBrowserController::OnReadIcon(SkBitmap bitmap) {
  if (bitmap.empty()) {
    DLOG(ERROR) << "Failed to read icon for web app";
    return;
  }

  app_icon_ =
      ui::ImageModel::FromImageSkia(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  if (auto* contents = web_contents())
    contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  if (callback_for_testing_)
    std::move(callback_for_testing_).Run();
}

void WebAppBrowserController::PerformDigitalAssetLinkVerification(
    Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  asset_link_handler_ =
      std::make_unique<digital_asset_links::DigitalAssetLinksHandler>(
          browser->profile()->GetURLLoaderFactory());
  is_verified_ = absl::nullopt;

  ash::ApkWebAppService* apk_web_app_service =
      ash::ApkWebAppService::Get(browser->profile());
  if (!apk_web_app_service || !apk_web_app_service->IsWebOnlyTwa(app_id()))
    return;

  const std::string origin = GetAppStartUrl().DeprecatedGetOriginAsURL().spec();
  const absl::optional<std::string> package_name =
      apk_web_app_service->GetPackageNameForWebApp(app_id());
  const absl::optional<std::string> fingerprint =
      apk_web_app_service->GetCertificateSha256Fingerprint(app_id());

  // Any web-only TWA should have an associated package name and fingerprint.
  DCHECK(package_name.has_value());
  DCHECK(fingerprint.has_value());

  // base::Unretained is safe as |asset_link_handler_| is owned by this object
  // and will be destroyed if this object is destroyed.
  asset_link_handler_->CheckDigitalAssetLinkRelationshipForAndroidApp(
      origin, kRelationship, fingerprint.value(), package_name.value(),
      base::BindOnce(&WebAppBrowserController::OnRelationshipCheckComplete,
                     base::Unretained(this)));
#endif
}

absl::optional<SkColor>
WebAppBrowserController::GetResolvedManifestBackgroundColor() const {
  if (ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    auto dark_mode_color = registrar().GetAppDarkModeBackgroundColor(app_id());
    if (dark_mode_color)
      return dark_mode_color;
  }
  return registrar().GetAppBackgroundColor(app_id());
}

}  // namespace web_app
