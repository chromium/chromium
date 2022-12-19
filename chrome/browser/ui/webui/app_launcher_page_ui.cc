// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_launcher_page_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/ntp/app_icon_webui_handler.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/app_resource_cache_factory.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/browser/ui/webui/theme_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/apps_resources.h"
#include "chrome/grit/apps_resources_map.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_urls.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/animation/animation.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/platform_util.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// AppLauncherPageUI

AppLauncherPageUI::AppLauncherPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(IDS_APP_LAUNCHER_TAB_TITLE));

  if (!GetProfile()->IsOffTheRecord()) {
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(GetProfile())->extension_service();
    web_app::WebAppProvider* web_app_provider =
        web_app::WebAppProvider::GetForWebApps(GetProfile());
    DCHECK(web_app_provider);
    DCHECK(service);
    // We should not be launched without an ExtensionService or WebAppProvider.
    web_ui->AddMessageHandler(
        std::make_unique<AppLauncherHandler>(service, web_app_provider));
    web_ui->AddMessageHandler(std::make_unique<CoreAppLauncherHandler>());
    web_ui->AddMessageHandler(std::make_unique<AppIconWebUIHandler>());
    web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
  }

  // The theme handler can require some CPU, so do it after hooking up the most
  // visited handler. This allows the DB query for the new tab thumbs to happen
  // earlier.
  web_ui->AddMessageHandler(std::make_unique<ThemeHandler>());

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      GetProfile(), chrome::kChromeUIAppLauncherPageHost);

  source->AddResourcePaths(base::make_span(kAppsResources, kAppsResourcesSize));
  source->SetDefaultResource(IDR_APPS_NEW_TAB_HTML);
  source->UseStringsJs();

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"title", IDS_NEW_TAB_TITLE},
      {"webStoreTitle", IDS_EXTENSION_WEB_STORE_TITLE},
      {"webStoreTitleShort", IDS_EXTENSION_WEB_STORE_TITLE_SHORT},
      {"attributionintro", IDS_NEW_TAB_ATTRIBUTION_INTRO},
      {"appuninstall", IDS_EXTENSIONS_UNINSTALL},
      {"appoptions", IDS_NEW_TAB_APP_OPTIONS},
      {"appdetails", IDS_NEW_TAB_APP_DETAILS},
      {"appinfodialog", IDS_APP_CONTEXT_MENU_SHOW_INFO},
      {"appcreateshortcut", IDS_NEW_TAB_APP_CREATE_SHORTCUT},
      {"appinstalllocally", IDS_NEW_TAB_APP_INSTALL_LOCALLY},
      {"appDefaultPageName", IDS_APP_DEFAULT_PAGE_NAME},
      {"applaunchtypepinned", IDS_APP_CONTEXT_MENU_OPEN_PINNED},
      {"applaunchtyperegular", IDS_APP_CONTEXT_MENU_OPEN_REGULAR},
      {"applaunchtypewindow", IDS_APP_CONTEXT_MENU_OPEN_WINDOW},
      {"applaunchtypefullscreen", IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN},
      {"syncpromotext", IDS_SYNC_START_SYNC_BUTTON_LABEL},
      {"syncLinkText", IDS_SYNC_ADVANCED_OPTIONS},
      {"learnMore", IDS_LEARN_MORE},
      {"appInstallHintText", IDS_NEW_TAB_APP_INSTALL_HINT_LABEL},
      {"learn_more", IDS_LEARN_MORE},
      {"tile_grid_screenreader_accessible_description",
       IDS_NEW_TAB_TILE_GRID_ACCESSIBLE_DESCRIPTION},
      {"page_switcher_change_title", IDS_NEW_TAB_PAGE_SWITCHER_CHANGE_TITLE},
      {"page_switcher_same_title", IDS_NEW_TAB_PAGE_SWITCHER_SAME_TITLE},
      {"runonoslogin", IDS_APP_CONTEXT_MENU_RUN_ON_OS_LOGIN},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  PrefService* prefs = GetProfile()->GetPrefs();
  source->AddString(
      "bookmarkbarattached",
      prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar) ? "true" : "false");

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  source->AddString("webStoreLink",
                    google_util::AppendGoogleLocaleParam(
                        extension_urls::GetWebstoreLaunchURL(), app_locale)
                        .spec());

  bool is_swipe_tracking_from_scroll_events_enabled = false;
#if BUILDFLAG(IS_MAC)
  // On the Mac, horizontal scrolling can be treated as a back or forward
  // gesture. Pass through a flag that indicates whether or not that feature is
  // enabled.
  is_swipe_tracking_from_scroll_events_enabled =
      platform_util::IsSwipeTrackingFromScrollEventsEnabled();
#endif
  source->AddBoolean("isSwipeTrackingFromScrollEventsEnabled",
                     is_swipe_tracking_from_scroll_events_enabled);

  source->AddBoolean("showWebStoreIcon",
                     !prefs->GetBoolean(prefs::kHideWebStoreIcon));

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kHideWebStoreIcon,
      base::BindRepeating(&AppLauncherPageUI::OnHideWebStoreIconChanged,
                          base::Unretained(this)));

  source->AddBoolean("canShowAppInfoDialog", CanPlatformShowAppInfoDialog());

  AppLauncherHandler::RegisterLoadTimeData(GetProfile(), source);

  // Control fade and resize animations.
  source->AddBoolean("anim", gfx::Animation::ShouldRenderRichAnimation());

  source->AddBoolean("isUserSignedIn",
                     IdentityManagerFactory::GetForProfile(GetProfile())
                         ->HasPrimaryAccount(signin::ConsentLevel::kSync));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval' "
      "'unsafe-inline';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome://resources chrome://theme "
      "'unsafe-inline';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' chrome://extension-icon chrome://app-icon chrome://theme "
      "chrome://resources data:;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types apps-page-js cr-ui-bubble-js-static "
      "parse-html-subset;");
}

AppLauncherPageUI::~AppLauncherPageUI() {
}

void AppLauncherPageUI::OnHideWebStoreIconChanged() {
  base::Value::Dict update;
  PrefService* prefs = GetProfile()->GetPrefs();
  update.Set("showWebStoreIcon", !prefs->GetBoolean(prefs::kHideWebStoreIcon));
  content::WebUIDataSource::Update(
      GetProfile(), chrome::kChromeUIAppLauncherPageHost, std::move(update));
}

// static
base::RefCountedMemory* AppLauncherPageUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_BOOKMARK_BAR_APPS_SHORTCUT,
                                    scale_factor);
}

Profile* AppLauncherPageUI::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}
