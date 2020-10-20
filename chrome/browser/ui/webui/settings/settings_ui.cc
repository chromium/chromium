// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/captions_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/extension_control_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/hats_handler.h"
#include "chrome/browser/ui/webui/settings/import_data_handler.h"
#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"
#include "chrome/browser/ui/webui/settings/on_startup_handler.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/safety_check_handler.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"
#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"
#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/site_settings_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/settings_resources.h"
#include "chrome/grit/settings_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/features.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "printing/buildflags/buildflags.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler_win.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/settings/incompatible_applications_handler_win.h"
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/token_util.h"
#endif
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/login/auth/password_visibility_utils.h"
#include "components/arc/arc_util.h"
#include "components/user_manager/user.h"
#include "ui/base/ui_base_features.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/webui/customize_themes/chrome_customize_themes_handler.h"
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"
#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/certificates_handler.h"
#elif defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/ui/webui/settings/native_certificates_handler.h"
#endif  // defined(USE_NSS_CERTS)

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/printing_handler.h"
#endif

namespace settings {

#if !BUILDFLAG(OPTIMIZE_WEBUI)
constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/settings/";
#endif

// static
void SettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kImportDialogAutofillFormData, true);
  registry->RegisterBooleanPref(prefs::kImportDialogBookmarks, true);
  registry->RegisterBooleanPref(prefs::kImportDialogHistory, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSavedPasswords, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSearchEngine, true);
}

web_app::AppRegistrar& GetRegistrarForProfile(Profile* profile) {
  return web_app::WebAppProvider::Get(profile)->registrar();
}

SettingsUI::SettingsUI(content::WebUI* web_ui)
    :
#if !defined(OS_CHROMEOS)
      ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      customize_themes_factory_receiver_(this),
#else  // !defined(OS_CHROMEOS)
      content::WebUIController(web_ui),
#endif
      webui_load_timer_(web_ui->GetWebContents(),
                        "Settings.LoadDocumentTime.MD",
                        "Settings.LoadCompletedTime.MD") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUISettingsHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");

  AddSettingsPageUIHandler(std::make_unique<AppearanceHandler>(web_ui));

#if defined(USE_NSS_CERTS)
  AddSettingsPageUIHandler(
      std::make_unique<certificate_manager::CertificatesHandler>());
#elif defined(OS_WIN) || defined(OS_MAC)
  AddSettingsPageUIHandler(std::make_unique<NativeCertificatesHandler>());
#endif  // defined(USE_NSS_CERTS)
#if defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(
      chromeos::cert_provisioning::CertificateProvisioningUiHandler::
          CreateForProfile(profile));
#endif

#if defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<AccessibilityMainHandler>());
#else
  AddSettingsPageUIHandler(
      std::make_unique<AccessibilityMainHandler>(profile->GetPrefs()));
#endif  // defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<ClearBrowsingDataHandler>(web_ui, profile));
  AddSettingsPageUIHandler(std::make_unique<SafetyCheckHandler>());
  AddSettingsPageUIHandler(std::make_unique<CookiesViewHandler>());
  AddSettingsPageUIHandler(std::make_unique<DownloadsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ExtensionControlHandler>());
  AddSettingsPageUIHandler(std::make_unique<FontHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ImportDataHandler>());
  AddSettingsPageUIHandler(std::make_unique<HatsHandler>());

#if defined(OS_WIN)
  AddSettingsPageUIHandler(std::make_unique<LanguagesHandler>());
#endif  // defined(OS_WIN)
#if defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<LanguagesHandler>(profile));
#endif  // defined(OS_CHROMEOS)

  AddSettingsPageUIHandler(
      std::make_unique<MediaDevicesSelectionHandler>(profile));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<MetricsReportingHandler>());
#endif
  AddSettingsPageUIHandler(std::make_unique<OnStartupHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<PeopleHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(std::make_unique<SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SecureDnsHandler>());
  AddSettingsPageUIHandler(std::make_unique<SiteSettingsHandler>(
      profile, GetRegistrarForProfile(profile)));
  AddSettingsPageUIHandler(std::make_unique<StartupPagesHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysPINHandler>());
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysResetHandler>());
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysCredentialHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<SecurityKeysBioEnrollmentHandler>());

#if defined(OS_WIN) || defined(OS_MAC)
  AddSettingsPageUIHandler(std::make_unique<CaptionsHandler>());
#endif

#if defined(OS_CHROMEOS)
  InitBrowserSettingsWebUIHandlers();
#else
  AddSettingsPageUIHandler(std::make_unique<DefaultBrowserHandler>());
  AddSettingsPageUIHandler(std::make_unique<ManageProfileHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SystemHandler>());
#endif

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<PrintingHandler>());
#endif

#if defined(OS_WIN)
  AddSettingsPageUIHandler(std::make_unique<ChromeCleanupHandler>(profile));
#endif  // defined(OS_WIN)

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  bool has_incompatible_applications =
      IncompatibleApplicationsUpdater::HasCachedApplications();
  html_source->AddBoolean("showIncompatibleApplications",
                          has_incompatible_applications);
  html_source->AddBoolean("hasAdminRights", HasAdminRights());

  if (has_incompatible_applications)
    AddSettingsPageUIHandler(
        std::make_unique<IncompatibleApplicationsHandler>());
#endif  // OS_WIN && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  html_source->AddBoolean("signinAllowed", !profile->IsGuestSession() &&
                                               profile->GetPrefs()->GetBoolean(
                                                   prefs::kSigninAllowed));

  html_source->AddBoolean(
      "safeBrowsingEnhancedEnabled",
      base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection));

  html_source->AddBoolean(
      "passwordsWeaknessCheck",
      base::FeatureList::IsEnabled(
          password_manager::features::kPasswordsWeaknessCheck));

  html_source->AddBoolean(
      "editPasswordsInSettings",
      base::FeatureList::IsEnabled(
          password_manager::features::kEditPasswordsInSettings));

  html_source->AddBoolean("showImportPasswords",
                          base::FeatureList::IsEnabled(
                              password_manager::features::kPasswordImport));

  html_source->AddBoolean(
      "enableAccountStorage",
      base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage));

  html_source->AddBoolean(
      "enableContentSettingsRedesign",
      base::FeatureList::IsEnabled(features::kContentSettingsRedesign));

#if defined(OS_WIN)
  html_source->AddBoolean(
      "safetyCheckChromeCleanerChildEnabled",
      base::FeatureList::IsEnabled(features::kSafetyCheckChromeCleanerChild));
#endif

#if defined(OS_CHROMEOS)
  html_source->AddBoolean("splitSettingsSyncEnabled",
                          chromeos::features::IsSplitSettingsSyncEnabled());
  html_source->AddBoolean("useBrowserSyncConsent",
                          chromeos::features::ShouldUseBrowserSyncConsent());

  html_source->AddBoolean(
      "userCannotManuallyEnterPassword",
      !chromeos::password_visibility::AccountHasUserFacingPassword(
          chromeos::ProfileHelper::Get()
              ->GetUserByProfile(profile)
              ->GetAccountId()));

  // This is the browser settings page.
  html_source->AddBoolean("isOSSettings", false);
#else   // defined(OS_CHROMEOS)
  html_source->AddBoolean(
      "profileThemeSelectorEnabled",
      base::FeatureList::IsEnabled(features::kProfilesUIRevamp));
#endif  // !defined(OS_CHROMEOS)

  AddSettingsPageUIHandler(std::make_unique<AboutHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ResetSettingsHandler>(profile));

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "compromisedPasswords", IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT);
  plural_string_handler->AddLocalizedString(
      "insecurePasswords", IDS_SETTINGS_INSECURE_PASSWORDS_COUNT);
  plural_string_handler->AddLocalizedString("weakPasswords",
                                            IDS_SETTINGS_WEAK_PASSWORDS_COUNT);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

#if BUILDFLAG(OPTIMIZE_WEBUI)
  webui::SetupBundledWebUIDataSource(html_source, "settings.js",
                                     IDR_SETTINGS_SETTINGS_ROLLUP_JS,
                                     IDR_SETTINGS_SETTINGS_V3_HTML);
  html_source->AddResourcePath("shared.rollup.js",
                               IDR_SETTINGS_SHARED_ROLLUP_JS);
  html_source->AddResourcePath("lazy_load.js",
                               IDR_SETTINGS_LAZY_LOAD_ROLLUP_JS);

  // Register SVG images that are purposefully not inlined in the HTML bundle
  // above.
  static constexpr webui::ResourcePath kSvgResources[] = {
      {"images/cookies_banner.svg", IDR_SETTINGS_IMAGES_COOKIES_BANNER_SVG},
      {"images/cookies_banner_dark.svg",
       IDR_SETTINGS_IMAGES_COOKIES_BANNER_DARK_SVG},
      {"images/permissions_banner.svg",
       IDR_SETTINGS_IMAGES_PERMISSIONS_BANNER_SVG},
      {"images/permissions_banner_dark.svg",
       IDR_SETTINGS_IMAGES_PERMISSIONS_BANNER_DARK_SVG},
      {"images/safe_browsing_banner.svg",
       IDR_SETTINGS_IMAGES_SAFE_BROWSING_BANNER_SVG},
      {"images/safe_browsing_banner_dark.svg",
       IDR_SETTINGS_IMAGES_SAFE_BROWSING_BANNER_DARK_SVG},
      {"images/sync_banner.svg", IDR_SETTINGS_IMAGES_SYNC_BANNER_SVG},
      {"images/sync_banner_dark.svg", IDR_SETTINGS_IMAGES_SYNC_BANNER_DARK_SVG},
      {"images/password_check_neutral.svg",
       IDR_SETTINGS_IMAGES_PASSWORD_CHECK_NEUTRAL_SVG},
      {"images/password_check_neutral_dark.svg",
       IDR_SETTINGS_IMAGES_PASSWORD_CHECK_NEUTRAL_DARK_SVG},
      {"images/password_check_positive.svg",
       IDR_SETTINGS_IMAGES_PASSWORD_CHECK_POSITIVE_SVG},
      {"images/password_check_positive_dark.svg",
       IDR_SETTINGS_IMAGES_PASSWORD_CHECK_POSITIVE_DARK_SVG},
  };
  webui::AddResourcePathsBulk(html_source, kSvgResources);

#else
  webui::SetupWebUIDataSource(
      html_source, base::make_span(kSettingsResources, kSettingsResourcesSize),
      kGeneratedPath, IDR_SETTINGS_SETTINGS_V3_HTML);
#endif

  AddLocalizedStrings(html_source, profile, web_ui->GetWebContents());

  ManagedUIHandler::Initialize(web_ui, html_source);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  TryShowHatsSurveyWithTimeout();
}

SettingsUI::~SettingsUI() = default;

#if defined(OS_CHROMEOS)
void SettingsUI::InitBrowserSettingsWebUIHandlers() {
  Profile* profile = Profile::FromWebUI(web_ui());

  // TODO(jamescook): Sort out how account management is split between Chrome OS
  // and browser settings.
  if (chromeos::IsAccountManagerAvailable(profile)) {
    chromeos::AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    chromeos::AccountManager* account_manager =
        factory->GetAccountManager(profile->GetPath().value());
    DCHECK(account_manager);

    web_ui()->AddMessageHandler(
        std::make_unique<chromeos::settings::AccountManagerUIHandler>(
            account_manager, IdentityManagerFactory::GetForProfile(profile)));
  }

  // MultideviceHandler is required in browser settings to show a special note
  // under the notification permission that is auto-granted for Android Messages
  // integration in ChromeOS.
  if (!profile->IsGuestSession()) {
    chromeos::android_sms::AndroidSmsService* android_sms_service =
        chromeos::android_sms::AndroidSmsServiceFactory::GetForBrowserContext(
            profile);
    chromeos::phonehub::PhoneHubManager* phone_hub_manager =
        chromeos::phonehub::PhoneHubManagerFactory::GetForProfile(profile);
    web_ui()->AddMessageHandler(
        std::make_unique<chromeos::settings::MultideviceHandler>(
            profile->GetPrefs(),
            chromeos::multidevice_setup::MultiDeviceSetupClientFactory::
                GetForProfile(profile),
            phone_hub_manager
                ? phone_hub_manager->GetNotificationAccessManager()
                : nullptr,
            android_sms_service
                ? android_sms_service->android_sms_pairing_state_tracker()
                : nullptr,
            android_sms_service ? android_sms_service->android_sms_app_manager()
                                : nullptr));
  }

  web_ui()->AddMessageHandler(
      std::make_unique<chromeos::settings::AndroidAppsHandler>(profile));
}
#else   // defined(OS_CHROMEOS)
void SettingsUI::BindInterface(
    mojo::PendingReceiver<
        customize_themes::mojom::CustomizeThemesHandlerFactory>
        pending_receiver) {
  if (customize_themes_factory_receiver_.is_bound())
    customize_themes_factory_receiver_.reset();
  customize_themes_factory_receiver_.Bind(std::move(pending_receiver));
}
#endif  // !defined(OS_CHROMEOS)

void SettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<content::WebUIMessageHandler> handler) {
  DCHECK(handler);
  web_ui()->AddMessageHandler(std::move(handler));
}

void SettingsUI::TryShowHatsSurveyWithTimeout() {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()),
                                        /* create_if_necessary = */ true);
  if (hats_service) {
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerSettings, web_ui()->GetWebContents(), 20000);
  }
}

#if !defined(OS_CHROMEOS)
void SettingsUI::CreateCustomizeThemesHandler(
    mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
        pending_client,
    mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
        pending_handler) {
  customize_themes_handler_ = std::make_unique<ChromeCustomizeThemesHandler>(
      std::move(pending_client), std::move(pending_handler),
      web_ui()->GetWebContents(), Profile::FromWebUI(web_ui()));
}
#endif  // !defined(OS_CHROMEOS)

WEB_UI_CONTROLLER_TYPE_IMPL(SettingsUI)

}  // namespace settings
