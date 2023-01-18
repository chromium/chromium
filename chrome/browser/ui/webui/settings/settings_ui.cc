// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/extension_control_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/hats_handler.h"
#include "chrome/browser/ui/webui/settings/import_data_handler.h"
#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"
#include "chrome/browser/ui/webui/settings/on_startup_handler.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/performance_handler.h"
#include "chrome/browser/ui/webui/settings/privacy_sandbox_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/safety_check_handler.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"
#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"
#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/site_settings_handler.h"
#include "chrome/browser/ui/webui/settings/site_settings_permissions_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/settings_resources.h"
#include "chrome/grit/settings_resources_map.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/content_settings/core/common/features.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/performance_manager/public/features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/features.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "crypto/crypto_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/interaction/element_identifier.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler_win.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/settings/incompatible_applications_handler_win.h"
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/token_util.h"
#endif
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/language/core/common/language_experiments.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/android_sms/android_sms_app_manager.h"
#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/settings/ash/account_manager_ui_handler.h"
#include "chrome/browser/ui/webui/settings/ash/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/ash/multidevice_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/login/auth/password_visibility_utils.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/user_manager/user.h"
#include "ui/base/ui_base_features.h"
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/webui/customize_themes/chrome_customize_themes_handler.h"
#include "chrome/browser/ui/webui/settings/captions_handler.h"
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"
#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/certificates_handler.h"
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/webui/settings/native_certificates_handler.h"
#endif  // BUILDFLAG(USE_NSS_CERTS)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#endif

namespace settings {

// static
void SettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kImportDialogAutofillFormData, true);
  registry->RegisterBooleanPref(prefs::kImportDialogBookmarks, true);
  registry->RegisterBooleanPref(prefs::kImportDialogHistory, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSavedPasswords, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSearchEngine, true);
}

SettingsUI::SettingsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Settings.LoadDocumentTime.MD",
                        "Settings.LoadCompletedTime.MD") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUISettingsHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");

  AddSettingsPageUIHandler(std::make_unique<AppearanceHandler>(web_ui));

#if BUILDFLAG(USE_NSS_CERTS)
  AddSettingsPageUIHandler(
      std::make_unique<certificate_manager::CertificatesHandler>());
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  AddSettingsPageUIHandler(std::make_unique<NativeCertificatesHandler>());
#endif  // BUILDFLAG(USE_NSS_CERTS)
#if BUILDFLAG(IS_CHROMEOS)
  AddSettingsPageUIHandler(
      chromeos::cert_provisioning::CertificateProvisioningUiHandler::
          CreateForProfile(profile));
#endif  // BUILDFLAG(IS_CHROMEOS)

  AddSettingsPageUIHandler(std::make_unique<AccessibilityMainHandler>());
  AddSettingsPageUIHandler(std::make_unique<BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<ClearBrowsingDataHandler>(web_ui, profile));
  AddSettingsPageUIHandler(std::make_unique<SafetyCheckHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<SiteSettingsPermissionsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<DownloadsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ExtensionControlHandler>());
  AddSettingsPageUIHandler(std::make_unique<FontHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ImportDataHandler>());
  AddSettingsPageUIHandler(std::make_unique<HatsHandler>());

#if BUILDFLAG(IS_WIN)
  AddSettingsPageUIHandler(std::make_unique<LanguagesHandler>());
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddSettingsPageUIHandler(std::make_unique<LanguagesHandler>(profile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  AddSettingsPageUIHandler(
      std::make_unique<MediaDevicesSelectionHandler>(profile));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
  AddSettingsPageUIHandler(std::make_unique<MetricsReportingHandler>());
#endif
  AddSettingsPageUIHandler(std::make_unique<OnStartupHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<PeopleHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProtocolHandlersHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<PrivacySandboxHandler>());
  AddSettingsPageUIHandler(std::make_unique<SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SecureDnsHandler>());
  AddSettingsPageUIHandler(std::make_unique<SiteSettingsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<StartupPagesHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysPINHandler>());
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysResetHandler>());
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysCredentialHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<SecurityKeysBioEnrollmentHandler>());
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysPhonesHandler>());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  AddSettingsPageUIHandler(std::make_unique<PasskeysHandler>());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  InitBrowserSettingsWebUIHandlers();
#else
  AddSettingsPageUIHandler(
      std::make_unique<CaptionsHandler>(profile->GetPrefs()));
  AddSettingsPageUIHandler(std::make_unique<DefaultBrowserHandler>());
  AddSettingsPageUIHandler(std::make_unique<ManageProfileHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SystemHandler>());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  html_source->AddBoolean("isSecondaryUser", !profile->IsMainProfile());
#endif

#endif

#if BUILDFLAG(IS_WIN)
  AddSettingsPageUIHandler(std::make_unique<ChromeCleanupHandler>(profile));
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  bool has_incompatible_applications =
      IncompatibleApplicationsUpdater::HasCachedApplications();
  html_source->AddBoolean("showIncompatibleApplications",
                          has_incompatible_applications);
  html_source->AddBoolean("hasAdminRights", HasAdminRights());

  if (has_incompatible_applications)
    AddSettingsPageUIHandler(
        std::make_unique<IncompatibleApplicationsHandler>());
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  html_source->AddBoolean("signinAllowed", !profile->IsGuestSession() &&
                                               profile->GetPrefs()->GetBoolean(
                                                   prefs::kSigninAllowed));

  html_source->AddBoolean(
      "turnOffSyncAllowedForManagedProfiles",
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));

  html_source->AddBoolean("showImportPasswords",
                          base::FeatureList::IsEnabled(
                              password_manager::features::kPasswordImport));

  html_source->AddBoolean(
      "enablePasswordViewPage",
      base::FeatureList::IsEnabled(
          password_manager::features::kPasswordViewPageInSettings) ||
          base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup));

  html_source->AddBoolean(
      "enablePasswordNotes",
      base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup));

  html_source->AddBoolean(
      "enableSendPasswords",
      base::FeatureList::IsEnabled(password_manager::features::kSendPasswords));

  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  html_source->AddBoolean("changePriceEmailNotificationsEnabled",
                          shopping_service->IsShoppingListEligible());
  if (shopping_service->IsShoppingListEligible()) {
    commerce::ShoppingServiceFactory::GetForBrowserContext(profile)
        ->FetchPriceEmailPref();
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddBoolean(
      "enableDesktopDetailedLanguageSettings",
      base::FeatureList::IsEnabled(language::kDesktopDetailedLanguageSettings));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddBoolean(
      "userCannotManuallyEnterPassword",
      !ash::password_visibility::AccountHasUserFacingPassword(
          g_browser_process->local_state(), ash::ProfileHelper::Get()
                                                ->GetUserByProfile(profile)
                                                ->GetAccountId()));

  // This is the browser settings page.
  html_source->AddBoolean("isOSSettings", false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros has no access to AccountHasUserFacingPassword() (Ash only). Assign
  // userCannotManuallyEnterPassword to false so that WebUI would make auth
  // token request, which is forwarded via crosapi to Ash, which then calls
  // AccountHasUserFacingPassword().
  html_source->AddBoolean("userCannotManuallyEnterPassword", false);
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddBoolean(
      "useSystemAuthenticationForPasswordManager",
      chromeos::features::IsPasswordManagerSystemAuthenticationEnabled());
#endif

  bool show_privacy_guide =
      !chrome::ShouldDisplayManagedUi(profile) && !profile->IsChild();
  html_source->AddBoolean("showPrivacyGuide", show_privacy_guide);

  html_source->AddBoolean("esbSettingsImprovementsEnabled",
                          base::FeatureList::IsEnabled(
                              safe_browsing::kEsbIphBubbleAndCollapseSettings));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  html_source->AddBoolean(
      "biometricAuthenticationForFilling",
      password_manager_util::
          ShouldBiometricAuthenticationForFillingToggleBeVisible(
              g_browser_process->local_state()));
#endif

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
  plural_string_handler->AddLocalizedString("securityKeysNewPIN",
                                            IDS_SETTINGS_SECURITY_KEYS_NEW_PIN);
  plural_string_handler->AddLocalizedString(
      "movePasswordsToAccount",
      IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT_COUNT);
  plural_string_handler->AddLocalizedString(
      "safetyCheckPasswordsCompromised",
      IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT_SHORT);
  plural_string_handler->AddLocalizedString(
      "safetyCheckPasswordsWeak", IDS_SETTINGS_WEAK_PASSWORDS_COUNT_SHORT);
  plural_string_handler->AddLocalizedString(
      "importPasswordsSuccessSummaryDevice",
      IDS_SETTINGS_PASSWORDS_IMPORT_SUCCESS_SUMMARY_DEVICE);
  plural_string_handler->AddLocalizedString(
      "importPasswordsSuccessSummaryAccount",
      IDS_SETTINGS_PASSWORDS_IMPORT_SUCCESS_SUMMARY_ACCOUNT);
  plural_string_handler->AddLocalizedString(
      "importPasswordsBadRowsFormat",
      IDS_SETTINGS_PASSWORDS_IMPORT_BAD_ROWS_FORMAT);
  plural_string_handler->AddLocalizedString(
      "safetyCheckNotificationPermissionReviewHeaderLabel",
      IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_HEADER_LABEL);
  plural_string_handler->AddLocalizedString(
      "safetyCheckNotificationPermissionReviewBlockAllToastLabel",
      IDS_SETTINGS_SAFETY_CHECK_NOTIFICATION_PERMISSION_REVIEW_BLOCK_ALL_TOAST_LABEL);
  plural_string_handler->AddLocalizedString(
      "safetyCheckNotificationPermissionReviewPrimaryLabel",
      IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_PRIMARY_LABEL);
  plural_string_handler->AddLocalizedString(
      "safetyCheckUnusedSitePermissionsHeaderLabel",
      IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_HEADER_LABEL);
  plural_string_handler->AddLocalizedString(
      "safetyCheckNotificationPermissionReviewSecondaryLabel",
      IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_SECONDARY_LABEL);
  plural_string_handler->AddLocalizedString(
      "safetyCheckUnusedSitePermissionsPrimaryLabel",
      IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_PRIMARY_LABEL);
  plural_string_handler->AddLocalizedString(
      "safetyCheckUnusedSitePermissionsSecondaryLabel",
      IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_SECONDARY_LABEL);
  plural_string_handler->AddLocalizedString(
      "safetyCheckUnusedSitePermissionsToastBulkLabel",
      IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_TOAST_BULK_LABEL);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  webui::SetupWebUIDataSource(
      html_source, base::make_span(kSettingsResources, kSettingsResourcesSize),
      IDR_SETTINGS_SETTINGS_HTML);

  AddLocalizedStrings(html_source, profile, web_ui->GetWebContents());

  ManagedUIHandler::Initialize(web_ui, html_source);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  // Privacy Sandbox
  bool is_privacy_sandbox_restricted =
      PrivacySandboxServiceFactory::GetForProfile(profile)
          ->IsPrivacySandboxRestricted();
  bool is_privacy_sandbox_settings_4 =
      base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4);
  html_source->AddBoolean("isPrivacySandboxRestricted",
                          is_privacy_sandbox_restricted);
  html_source->AddBoolean("isPrivacySandboxSettings4",
                          is_privacy_sandbox_settings_4);
  if (!is_privacy_sandbox_restricted && !is_privacy_sandbox_settings_4) {
    html_source->AddResourcePath(
        "privacySandbox", IDR_SETTINGS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_HTML);
  }

  html_source->AddBoolean("safetyCheckNotificationPermissionsEnabled",
                          base::FeatureList::IsEnabled(
                              features::kSafetyCheckNotificationPermissions));
  html_source->AddBoolean(
      "safetyCheckUnusedSitePermissionsEnabled",
      base::FeatureList::IsEnabled(
          content_settings::features::kSafetyCheckUnusedSitePermissions));

  // Performance
  AddSettingsPageUIHandler(std::make_unique<PerformanceHandler>());
  html_source->AddBoolean(
      "highEfficiencyModeAvailable",
      base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable));
  html_source->AddBoolean(
      "batterySaverModeAvailable",
      base::FeatureList::IsEnabled(
          performance_manager::features::kBatterySaverModeAvailable));

  TryShowHatsSurveyWithTimeout();
}

SettingsUI::~SettingsUI() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SettingsUI::InitBrowserSettingsWebUIHandlers() {
  Profile* profile = Profile::FromWebUI(web_ui());

  // TODO(jamescook): Sort out how account management is split between Chrome OS
  // and browser settings.
  if (ash::IsAccountManagerAvailable(profile)) {
    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    auto* account_manager =
        factory->GetAccountManager(profile->GetPath().value());
    DCHECK(account_manager);
    auto* account_manager_facade =
        ::GetAccountManagerFacade(profile->GetPath().value());
    DCHECK(account_manager_facade);

    web_ui()->AddMessageHandler(
        std::make_unique<ash::settings::AccountManagerUIHandler>(
            account_manager, account_manager_facade,
            IdentityManagerFactory::GetForProfile(profile),
            ash::AccountAppsAvailabilityFactory::GetForProfile(profile)));
  }

  // MultideviceHandler is required in browser settings to show a special note
  // under the notification permission that is auto-granted for Android Messages
  // integration in ChromeOS.
  if (!profile->IsGuestSession()) {
    auto* android_sms_service =
        ash::android_sms::AndroidSmsServiceFactory::GetForBrowserContext(
            profile);
    ash::phonehub::PhoneHubManager* phone_hub_manager =
        ash::phonehub::PhoneHubManagerFactory::GetForProfile(profile);
    ash::eche_app::EcheAppManager* eche_app_manager =
        ash::eche_app::EcheAppManagerFactory::GetForProfile(profile);
    web_ui()->AddMessageHandler(std::make_unique<
                                ash::settings::MultideviceHandler>(
        profile->GetPrefs(),
        ash::multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            profile),
        phone_hub_manager
            ? phone_hub_manager->GetMultideviceFeatureAccessManager()
            : nullptr,
        android_sms_service
            ? android_sms_service->android_sms_pairing_state_tracker()
            : nullptr,
        android_sms_service ? android_sms_service->android_sms_app_manager()
                            : nullptr,
        eche_app_manager ? eche_app_manager->GetAppsAccessManager() : nullptr,
        phone_hub_manager ? phone_hub_manager->GetCameraRollManager()
                          : nullptr));
  }
}
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
void SettingsUI::BindInterface(
    mojo::PendingReceiver<
        customize_themes::mojom::CustomizeThemesHandlerFactory>
        pending_receiver) {
  if (customize_themes_factory_receiver_.is_bound())
    customize_themes_factory_receiver_.reset();
  customize_themes_factory_receiver_.Bind(std::move(pending_receiver));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void SettingsUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound())
    help_bubble_handler_factory_receiver_.reset();
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SettingsUI::CreateCustomizeThemesHandler(
    mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
        pending_client,
    mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
        pending_handler) {
  customize_themes_handler_ = std::make_unique<ChromeCustomizeThemesHandler>(
      std::move(pending_client), std::move(pending_handler),
      web_ui()->GetWebContents(), Profile::FromWebUI(web_ui()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void SettingsUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{kEnhancedProtectionSettingElementId});
}

WEB_UI_CONTROLLER_TYPE_IMPL(SettingsUI)

}  // namespace settings
