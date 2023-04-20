// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"

#include "base/check_is_test.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
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
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/system_web_apps/color_helpers.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace {

const int kMinimumHomeTabIconSizeInPx = 16;

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kRelationship[] = "delegate_permission/common.handle_all_urls";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// SystemWebAppDelegate provides menu.
class SystemAppTabMenuModelFactory : public TabMenuModelFactory {
 public:
  explicit SystemAppTabMenuModelFactory(
      const ash::SystemWebAppDelegate* system_app)
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
  raw_ptr<const ash::SystemWebAppDelegate> system_app_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::OnceClosure& IconLoadCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

base::OnceClosure& ManifestUpdateAppliedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

}  // namespace

namespace web_app {

WebAppBrowserController::WebAppBrowserController(
    WebAppProvider& provider,
    Browser* browser,
    AppId app_id,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const ash::SystemWebAppDelegate* system_app,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    bool has_tab_strip)
    : AppBrowserController(browser, std::move(app_id), has_tab_strip),
      provider_(provider)
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      system_app_(system_app)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
{
  manifest_display_mode_ =
      registrar().GetEffectiveDisplayModeFromManifest(this->app_id());
  effective_display_mode_ =
      registrar().GetAppEffectiveDisplayMode(this->app_id());
  install_manager_observation_.Observe(&provider.install_manager());
  PerformDigitalAssetLinkVerification(browser);
}

WebAppBrowserController::~WebAppBrowserController() = default;

bool WebAppBrowserController::HasMinimalUiButtons() const {
  if (has_tab_strip())
    return false;
  return manifest_display_mode_ == DisplayMode::kBrowser ||
         manifest_display_mode_ == DisplayMode::kMinimalUi;
}

bool WebAppBrowserController::IsHostedApp() const {
  return true;
}

std::unique_ptr<TabMenuModelFactory>
WebAppBrowserController::GetTabMenuModelFactory() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (system_app() && system_app()->HasCustomTabMenuModel()) {
    return std::make_unique<SystemAppTabMenuModelFactory>(system_app());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return nullptr;
}

bool WebAppBrowserController::AppUsesWindowControlsOverlay() const {
  return effective_display_mode_ == DisplayMode::kWindowControlsOverlay;
}

bool WebAppBrowserController::IsWindowControlsOverlayEnabled() const {
  return AppUsesWindowControlsOverlay() &&
         registrar().GetWindowControlsOverlayEnabled(app_id());
}

void WebAppBrowserController::ToggleWindowControlsOverlayEnabled(
    base::OnceClosure on_complete) {
  DCHECK(AppUsesWindowControlsOverlay());

  provider_->scheduler().ScheduleCallbackWithLock<AppLock>(
      "WebAppBrowserController::ToggleWindowControlsOverlayEnabled",
      std::make_unique<AppLockDescription>(app_id()),
      base::BindOnce(
          [](base::OnceClosure on_complete, const AppId& app_id,
             AppLock& lock) {
            lock.sync_bridge().SetAppWindowControlsOverlayEnabled(
                app_id,
                !lock.registrar().GetWindowControlsOverlayEnabled(app_id));
            std::move(on_complete).Run();
          },
          std::move(on_complete), app_id()));
}

bool WebAppBrowserController::AppUsesBorderlessMode() const {
  return IsIsolatedWebApp() &&
         effective_display_mode_ == DisplayMode::kBorderless;
}

bool WebAppBrowserController::AppUsesTabbed() const {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip)) {
    return false;
  }
  return effective_display_mode_ == DisplayMode::kTabbed;
}

bool WebAppBrowserController::IsIsolatedWebApp() const {
  return is_isolated_web_app_for_testing_ || registrar().IsIsolated(app_id());
}

void WebAppBrowserController::SetIsolatedWebAppTrueForTesting() {
  is_isolated_web_app_for_testing_ = true;
}

gfx::Rect WebAppBrowserController::GetDefaultBounds() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (system_app_) {
    return system_app_->GetDefaultBounds(browser());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return gfx::Rect();
}

bool WebAppBrowserController::HasReloadButton() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (system_app_)
    return system_app_->ShouldHaveReloadButtonInMinimalUi();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
}

#if !BUILDFLAG(IS_CHROMEOS)
bool WebAppBrowserController::HasProfileMenuButton() const {
  return (app_id() == web_app::kPasswordManagerAppId) &&
         base::FeatureList::IsEnabled(
             password_manager::features::kPasswordManagerRedesign);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const ash::SystemWebAppDelegate* WebAppBrowserController::system_app() const {
  return system_app_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
bool WebAppBrowserController::AlwaysShowToolbarInFullscreen() const {
  // Reading this setting synchronously rather than going through the command
  // manager greatly simplifies where this is read. This should be fine, since
  // this is only persisted in the web app db.
  return registrar().AlwaysShowToolbarInFullscreen(app_id());
}

void WebAppBrowserController::ToggleAlwaysShowToolbarInFullscreen() {
  provider_->scheduler().ScheduleCallbackWithLock<AppLock>(
      "WebAppBrowserController::ToggleAlwaysShowToolbarInFullscreen",
      std::make_unique<AppLockDescription>(app_id()),
      base::BindOnce(
          [](const AppId& app_id, AppLock& lock) {
            lock.sync_bridge().SetAlwaysShowToolbarInFullscreen(
                app_id,
                !lock.registrar().AlwaysShowToolbarInFullscreen(app_id));
          },
          app_id()));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
bool WebAppBrowserController::ShouldShowCustomTabBar() const {
  if (AppBrowserController::ShouldShowCustomTabBar())
    return true;

  return is_verified_.value_or(false);
}

void WebAppBrowserController::CheckDigitalAssetLinkRelationshipForAndroidApp(
    const std::string& package_name,
    const std::string& fingerprint) {
  // base::Unretained is safe as |asset_link_handler_| is owned by this object
  // and will be destroyed if this object is destroyed.
  // TODO(swestphal): Support passing several fingerprints for verification.
  std::vector<std::string> fingerprints{fingerprint};
  asset_link_handler_->CheckDigitalAssetLinkRelationshipForAndroidApp(
      url::Origin::Create(GetAppStartUrl()), kRelationship,
      std::move(fingerprints), package_name,
      base::BindOnce(&WebAppBrowserController::OnRelationshipCheckComplete,
                     base::Unretained(this)));
}

void WebAppBrowserController::OnRelationshipCheckComplete(
    content_relationship_verification::RelationshipCheckResult result) {
  bool should_show_cct = false;
  switch (result) {
    case content_relationship_verification::RelationshipCheckResult::kSuccess:
      should_show_cct = false;
      break;
    case content_relationship_verification::RelationshipCheckResult::kFailure:
    case content_relationship_verification::RelationshipCheckResult::
        kNoConnection:
      should_show_cct = true;
      break;
  }
  is_verified_ = should_show_cct;
  browser()->window()->UpdateCustomTabBarVisibility(should_show_cct,
                                                    false /* animate */);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void WebAppBrowserController::OnGetAssociatedAndroidPackage(
    crosapi::mojom::WebAppAndroidPackagePtr package) {
  if (!package) {
    // Web app was not installed from an Android package, nothing to check.
    return;
  }
  CheckDigitalAssetLinkRelationshipForAndroidApp(package->package_name,
                                                 package->sha256_fingerprint);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void WebAppBrowserController::OnWebAppUninstalled(
    const AppId& uninstalled_app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (uninstalled_app_id == app_id())
    chrome::CloseWindow(browser());
}

void WebAppBrowserController::OnWebAppManifestUpdated(
    const AppId& updated_app_id,
    base::StringPiece old_name) {
  if (updated_app_id == app_id()) {
    UpdateThemePack();
    app_icon_.reset();
    browser()->window()->UpdateTitleBar();

    if (ManifestUpdateAppliedCallbackForTesting()) {
      std::move(ManifestUpdateAppliedCallbackForTesting()).Run();
    }
  }
}

void WebAppBrowserController::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
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

  if (provider_->icon_manager().HasSmallestIcon(app_id(), {IconPurpose::ANY},
                                                kWebAppIconSmall)) {
    provider_->icon_manager().ReadSmallestIcon(
        app_id(), {IconPurpose::ANY}, kWebAppIconSmall,
        base::BindOnce(&WebAppBrowserController::OnReadIcon,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  return *app_icon_;
}

bool WebAppBrowserController::DoesHomeTabIconExist() const {
  const web_app::WebApp* web_app = registrar().GetAppById(app_id());
  if (web_app && web_app->tab_strip()) {
    web_app::TabStrip tab_strip = web_app->tab_strip().value();
    if (const auto* params =
            absl::get_if<blink::Manifest::HomeTabParams>(&tab_strip.home_tab)) {
      return !params->icons.empty();
    }
  }
  return false;
}

gfx::ImageSkia WebAppBrowserController::GetHomeTabIcon() const {
  if (home_tab_icon_) {
    return *home_tab_icon_;
  }

  const web_app::WebApp* web_app = registrar().GetAppById(app_id());
  if (web_app && web_app->tab_strip()) {
    web_app::TabStrip tab_strip = web_app->tab_strip().value();
    if (const auto* params =
            absl::get_if<blink::Manifest::HomeTabParams>(&tab_strip.home_tab)) {
      if (!params->icons.empty()) {
        provider_->icon_manager().ReadBestHomeTabIcon(
            app_id(), params->icons, kMinimumHomeTabIconSizeInPx,
            base::BindOnce(&WebAppBrowserController::OnReadHomeTabIcon,
                           weak_ptr_factory_.GetWeakPtr()));
      }
    }
  }
  if (!home_tab_icon_) {
    home_tab_icon_ = provider_->icon_manager().GetMonochromeFavicon(app_id());
  }
  if (home_tab_icon_->width() == 0 || home_tab_icon_->height() == 0) {
    home_tab_icon_ = *(GetWindowAppIcon().GetImage().ToImageSkia());
  }
  return *home_tab_icon_;
}

ui::ImageModel WebAppBrowserController::GetWindowIcon() const {
  return GetWindowAppIcon();
}

absl::optional<SkColor> WebAppBrowserController::GetThemeColor() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // System App popups (settings pages) always use default theme.
  if (system_app() && browser()->is_type_app_popup())
    return absl::nullopt;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  absl::optional<SkColor> web_theme_color =
      AppBrowserController::GetThemeColor();
  if (web_theme_color)
    return web_theme_color;

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    if (absl::optional<SkColor> fallback_page_theme_color =
            ChromeOsWebAppExperiments::GetFallbackPageThemeColor(
                app_id(),
                browser()->tab_strip_model()->GetActiveWebContents())) {
      return fallback_page_theme_color;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // System Apps with dynamic color ignore manifest and pull theme color from
  // the OS.
  if (system_app() && system_app()->UseSystemThemeColor() &&
      chromeos::features::IsJellyEnabled()) {
    return ash::GetSystemThemeColor();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  // Prefer an available web contents color but when such a color is
  // unavailable (i.e. in the time between when a window launches and it's web
  // content loads) attempt to pull the background color from the manifest.
  absl::optional<SkColor> result =
      web_contents_color ? web_contents_color : manifest_color;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (system_app()) {
    if (chromeos::features::IsJellyEnabled()) {
      // System Apps with dynamic color ignore the manifest and pull background
      // color from the OS in situations where a background color can not be
      // extracted from the web contents.
      SkColor os_color = ash::GetSystemBackgroundColor();

      result = web_contents_color ? web_contents_color : os_color;
    } else if (system_app()->PreferManifestBackgroundColor()) {
      // Some system web apps prefer their web content background color to be
      // ignored in favour of their manifest background color.
      result = manifest_color ? manifest_color : web_contents_color;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return result;
}

GURL WebAppBrowserController::GetAppStartUrl() const {
  return registrar().GetAppStartUrl(app_id());
}

GURL WebAppBrowserController::GetAppNewTabUrl() const {
  return registrar().GetAppNewTabUrl(app_id());
}

bool WebAppBrowserController::IsUrlInHomeTabScope(const GURL& url) const {
  if (!registrar().IsTabbedWindowModeEnabled(app_id())) {
    return false;
  }

  if (!IsUrlInAppScope(url)) {
    return false;
  }

  absl::optional<GURL> pinned_home_url =
      registrar().GetAppPinnedHomeTabUrl(app_id());
  if (!pinned_home_url) {
    return false;
  }

  // We ignore query params and hash ref when deciding what should be
  // opened as the home tab.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  if (url.ReplaceComponents(replacements) ==
      pinned_home_url.value().ReplaceComponents(replacements)) {
    return true;
  }

  if (!home_tab_scope_.has_value()) {
    home_tab_scope_ = GetTabbedHomeTabScope();
  }

  if (home_tab_scope_.has_value()) {
    std::vector<int> vec;
    return home_tab_scope_.value().Match(url.path(), &vec);
  }
  return false;
}

bool WebAppBrowserController::IsUrlInAppScope(const GURL& url) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (system_app() && system_app()->IsUrlInSystemAppScope(url))
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    size_t extended_scope_score =
        ChromeOsWebAppExperiments::GetExtendedScopeScore(app_id(), url.spec());
    if (extended_scope_score > 0)
      return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  std::u16string app_name = base::UTF8ToUTF16(
      provider_->registrar_unsafe().GetAppShortName(app_id()));
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
  return WebAppUiManagerImpl::Get(&*provider_)
      ->dialog_manager()
      .CanUserUninstallWebApp(app_id());
}

void WebAppBrowserController::Uninstall(
    webapps::WebappUninstallSource webapp_uninstall_source) {
  WebAppUiManagerImpl::Get(&*provider_)
      ->dialog_manager()
      .UninstallWebApp(app_id(), webapps::WebappUninstallSource::kAppMenu,
                       browser()->window(), base::DoNothing());
}

bool WebAppBrowserController::IsInstalled() const {
  return registrar().IsInstalled(app_id());
}

base::CallbackListSubscription
WebAppBrowserController::AddHomeTabIconLoadCallbackForTesting(
    base::OnceClosure callback) {
  return home_tab_callback_list_.Add(std::move(callback));
}

void WebAppBrowserController::SetIconLoadCallbackForTesting(
    base::OnceClosure callback) {
  IconLoadCallbackForTesting() = std::move(callback);
}

void WebAppBrowserController::SetManifestUpdateAppliedCallbackForTesting(
    base::OnceClosure callback) {
  ManifestUpdateAppliedCallbackForTesting() = std::move(callback);
}

void WebAppBrowserController::OnTabInserted(content::WebContents* contents) {
  AppBrowserController::OnTabInserted(contents);
  SetAppPrefsForWebContents(contents);

  // If a `WebContents` is inserted into an app browser (e.g. after
  // installation), it is "appy". Note that if and when it's moved back into a
  // tabbed browser window (e.g. via "Open in Chrome" menu item), it is still
  // considered "appy".
  WebAppTabHelper::FromWebContents(contents)->set_acting_as_app(true);

  if (AppUsesTabbed() && IsUrlInHomeTabScope(contents->GetLastCommittedURL())) {
    WebAppTabHelper::FromWebContents(contents)->set_is_pinned_home_tab(true);
  }
}

void WebAppBrowserController::OnTabRemoved(content::WebContents* contents) {
  AppBrowserController::OnTabRemoved(contents);
  ClearAppPrefsForWebContents(contents);
}

const WebAppRegistrar& WebAppBrowserController::registrar() const {
  return provider_->registrar_unsafe();
}

const WebAppInstallManager& WebAppBrowserController::install_manager() const {
  return provider_->install_manager();
}

void WebAppBrowserController::LoadAppIcon(bool allow_placeholder_icon) const {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->LoadIcon(proxy->AppRegistryCache().GetAppType(app_id()), app_id(),
                  apps::IconType::kStandard, kWebAppIconSmall,
                  allow_placeholder_icon,
                  base::BindOnce(&WebAppBrowserController::OnLoadIcon,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void WebAppBrowserController::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  app_icon_ = ui::ImageModel::FromImageSkia(icon_value->uncompressed);

  if (icon_value->is_placeholder_icon)
    LoadAppIcon(false /* allow_placeholder_icon */);

  if (auto* contents = web_contents())
    contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  if (IconLoadCallbackForTesting()) {
    std::move(IconLoadCallbackForTesting()).Run();
  }
}

void WebAppBrowserController::OnReadHomeTabIcon(
    SkBitmap home_tab_icon_bitmap) const {
  if (home_tab_icon_bitmap.empty()) {
    DLOG(ERROR) << "Failed to read icon for the pinned home tab";
    return;
  }

  home_tab_icon_ = gfx::ImageSkia::CreateFrom1xBitmap(home_tab_icon_bitmap);
  if (auto* contents = web_contents()) {
    contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }

  home_tab_callback_list_.Notify();
}

void WebAppBrowserController::OnReadIcon(IconPurpose purpose, SkBitmap bitmap) {
  // We request only IconPurpose::ANY icons.
  DCHECK_EQ(purpose, IconPurpose::ANY);

  if (bitmap.empty()) {
    DLOG(ERROR) << "Failed to read icon for web app";
    return;
  }

  app_icon_ =
      ui::ImageModel::FromImageSkia(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  if (auto* contents = web_contents())
    contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  if (IconLoadCallbackForTesting()) {
    std::move(IconLoadCallbackForTesting()).Run();
  }
}

void WebAppBrowserController::PerformDigitalAssetLinkVerification(
    Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS)
  asset_link_handler_ = std::make_unique<
      content_relationship_verification::DigitalAssetLinksHandler>(
      browser->profile()->GetURLLoaderFactory());
  is_verified_ = absl::nullopt;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ApkWebAppService* apk_web_app_service =
      ash::ApkWebAppService::Get(browser->profile());
  if (!apk_web_app_service || !apk_web_app_service->IsWebOnlyTwa(app_id()))
    return;

  const absl::optional<std::string> package_name =
      apk_web_app_service->GetPackageNameForWebApp(app_id());
  const absl::optional<std::string> fingerprint =
      apk_web_app_service->GetCertificateSha256Fingerprint(app_id());

  // Any web-only TWA should have an associated package name and fingerprint.
  DCHECK(package_name.has_value());
  DCHECK(fingerprint.has_value());

  CheckDigitalAssetLinkRelationshipForAndroidApp(*package_name, *fingerprint);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (chromeos::BrowserParamsProxy::Get()->WebAppsEnabled() && lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::WebAppService>() &&
      lacros_service->GetInterfaceVersion(
          crosapi::mojom::WebAppService::Uuid_) >=
          int{crosapi::mojom::WebAppService::MethodMinVersions::
                  kGetAssociatedAndroidPackageMinVersion}) {
    lacros_service->GetRemote<crosapi::mojom::WebAppService>()
        ->GetAssociatedAndroidPackage(
            app_id(),
            base::BindOnce(
                &WebAppBrowserController::OnGetAssociatedAndroidPackage,
                weak_ptr_factory_.GetWeakPtr()));
  }
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

absl::optional<RE2::Set> WebAppBrowserController::GetTabbedHomeTabScope()
    const {
  const WebApp* web_app = registrar().GetAppById(app_id());
  if (!web_app) {
    return absl::nullopt;
  }
  TabStrip tab_strip = web_app->tab_strip().value();
  if (const auto* params =
          absl::get_if<blink::Manifest::HomeTabParams>(&tab_strip.home_tab)) {
    std::vector<blink::UrlPattern> scope_patterns = params->scope_patterns;

    RE2::Set scope_set = RE2::Set(RE2::Options(), RE2::Anchor::UNANCHORED);
    for (auto& scope : scope_patterns) {
      liburlpattern::Options options = {.delimiter_list = "/",
                                        .prefix_list = "/",
                                        .sensitive = true,
                                        .strict = false};
      liburlpattern::Pattern pattern(scope.pathname, options, "[^/]+?");
      std::string error;
      scope_set.Add(pattern.GenerateRegexString(), &error);
    }

    if (scope_set.Compile()) {
      return scope_set;
    }
  }
  return absl::nullopt;
}

}  // namespace web_app
