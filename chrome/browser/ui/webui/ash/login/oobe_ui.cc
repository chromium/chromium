// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/account_selection_screen_handler.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/esim_manager.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/shell.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/about/about_ui.h"
#include "chrome/browser/ui/webui/ash/login/add_child_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/categories_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/debug/debug_overlay_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hardware_data_collection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/install_attributes_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_state_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/management_transition_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/multidevice_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_dropdown_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/online_authentication_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_display_chooser.h"
#include "chrome/browser/ui/webui/ash/login/oobe_screens_handler_factory.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_trial_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/apply_online_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/factor_setup_success_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/osauth_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/parental_handoff_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/personalized_recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/remote_activity_notification_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/smart_privacy_protection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/split_modifier_keyboard_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/ssh_configured_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/testapi/oobe_test_api_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_allowlist_check_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/wrong_hwid_screen_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/test_files_request_filter.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/assistant_optin_resources.h"
#include "chrome/grit/assistant_optin_resources_map.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/gaia_auth_host_resources.h"
#include "chrome/grit/gaia_auth_host_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/oobe_resources.h"
#include "chrome/grit/oobe_resources_map.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "remoting/host/chromeos/features.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {

namespace {

// Sorted
constexpr char kArcOverlayCSSPath[] = "arc_support/overlay.css";
constexpr char kArcPlaystoreCSSPath[] = "arc_support/playstore.css";
constexpr char kArcPlaystoreJSPath[] = "arc_support/playstore.js";
constexpr char kArcPlaystoreLogoPath[] = "arc_support/icon/playstore.svg";
constexpr char kDebuggerMJSPath[] = "debug/debug.js";
constexpr char kQuickStartDebuggerPath[] = "debug/quick_start_debugger.js";
constexpr char kQuickStartDebuggerHtmlPath[] =
    "debug/quick_start_debugger.html.js";

constexpr char kProductLogoPath[] = "product-logo.png";
constexpr char kTestAPIJsMPath[] = "test_api/test_api.js";

// Components
constexpr char kOobeCustomVarsCssJs[] =
    "components/oobe_vars/oobe_custom_vars.css.js";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kLogo24PX1XSvgPath[] = "logo_24px-1x.svg";
constexpr char kLogo24PX2XSvgPath[] = "logo_24px-2x.svg";
constexpr char kSyncConsentIcons[] = "sync-consent-icons.html";
constexpr char kSyncConsentIconsJs[] = "sync-consent-icons.m.js";
constexpr char kWelcomeBackdrop[] = "internal_assets/welcome_backdrop.svg";
#endif

// Adds various product logo resources.
void AddProductLogoResources(content::WebUIDataSource* source) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath(kLogo24PX1XSvgPath, IDR_PRODUCT_LOGO_24PX_1X);
  source->AddResourcePath(kLogo24PX2XSvgPath, IDR_PRODUCT_LOGO_24PX_2X);
#endif

  // Required in encryption migration screen.
  source->AddResourcePath(kProductLogoPath, IDR_PRODUCT_LOGO_64);
}

void AddBootAnimationResources(content::WebUIDataSource* source) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath(kWelcomeBackdrop, IDR_CROS_OOBE_WELCOME_BACKDROP);
#endif
}

void AddSyncConsentResources(content::WebUIDataSource* source) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath(kSyncConsentIcons,
                          IDR_PRODUCT_CHROMEOS_SYNC_CONSENT_SCREEN_ICONS);
  source->AddResourcePath(kSyncConsentIconsJs,
                          IDR_PRODUCT_CHROMEOS_SYNC_CONSENT_SCREEN_ICONS_M_JS);
  // No #else section here as Sync Settings screen is Chrome-specific.
#endif
}

// Adds resources for ARC-dependent screens (PlayStore ToS, Assistant, etc...)
void AddArcScreensResources(content::WebUIDataSource* source) {
  // Required for postprocessing of Goolge PlayStore Terms and Overlay help.
  source->AddResourcePath(kArcOverlayCSSPath, IDR_ARC_SUPPORT_OVERLAY_CSS);
  source->AddResourcePath(kArcPlaystoreCSSPath, IDR_ARC_SUPPORT_PLAYSTORE_CSS);
  source->AddResourcePath(kArcPlaystoreJSPath, IDR_ARC_SUPPORT_PLAYSTORE_JS);
  source->AddResourcePath(kArcPlaystoreLogoPath,
                          IDR_ARC_SUPPORT_PLAYSTORE_LOGO);
}

void AddAssistantScreensResources(content::WebUIDataSource* source) {
  source->AddResourcePaths(
      base::make_span(kAssistantOptinResources, kAssistantOptinResourcesSize));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");
}

void AddMultiDeviceSetupResources(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");
}

void AddDebuggerResources(content::WebUIDataSource* source) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool dev_overlay_enabled =
      command_line->HasSwitch(switches::kShowOobeDevOverlay);
  const bool quick_start_debugger_enabled =
      command_line->HasSwitch(switches::kShowOobeQuickStartDebugger);
  // Enable for ChromeOS-on-linux for developers and test images.
  if (dev_overlay_enabled && base::SysInfo::IsRunningOnChromeOS()) {
    LOG(WARNING) << "OOBE Debug overlay can only be used on test images";
    base::SysInfo::CrashIfChromeOSNonTestImage();
  }

  source->AddResourcePath(kDebuggerMJSPath, dev_overlay_enabled
                                                ? IDR_OOBE_DEBUG_DEBUG_JS
                                                : IDR_OOBE_DEBUG_NO_DEBUG_JS);

  source->AddResourcePath(kQuickStartDebuggerPath,
                          quick_start_debugger_enabled
                              ? IDR_OOBE_DEBUG_QUICK_START_DEBUGGER_JS
                              : IDR_OOBE_DEBUG_NO_DEBUG_JS);
  if (quick_start_debugger_enabled) {
    source->AddResourcePath(kQuickStartDebuggerHtmlPath,
                            IDR_OOBE_DEBUG_QUICK_START_DEBUGGER_HTML_JS);
  }
}

void AddTestAPIResources(content::WebUIDataSource* source) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool enabled = command_line->HasSwitch(switches::kEnableOobeTestAPI);

  source->AddResourcePath(kTestAPIJsMPath,
                          enabled ? IDR_OOBE_TEST_API_TEST_API_JS
                                  : IDR_OOBE_TEST_API_NO_TEST_API_JS);
}

// Creates a WebUIDataSource for chrome://oobe
void CreateAndAddOobeUIDataSource(Profile* profile,
                                  const base::Value::Dict& localized_strings,
                                  const std::string& display_type) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIOobeHost);
  ash::EnableTrustedTypesCSP(source);
  source->AddLocalizedStrings(localized_strings);
  source->UseStringsJs();

  OobeUI::AddOobeComponents(source);

  source->SetDefaultResource(IDR_OOBE_OOBE_HTML);

  // Add boolean variables that are used to add screens
  // dynamically depending on the flow type.
  const bool is_oobe_flow = display_type == OobeUI::kOobeDisplay;

  if (display_type == OobeUI::kOobeTestLoader) {
    source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
    source->AddResourcePath("test_loader_util.js",
                            IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
    source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  }

  source->AddBoolean("isOsInstallAllowed", switches::IsOsInstallAllowed());
  source->AddBoolean("isOobeFlow", is_oobe_flow);
  source->AddBoolean("isOobeLazyLoadingEnabled",
                     features::IsOobeLazyLoadingEnabled());
  source->AddBoolean("isOobeAiIntroEnabled", features::IsOobeAiIntroEnabled());
  source->AddBoolean("isOobeGeminiIntroEnabled",
                     features::IsOobeGeminiIntroEnabled());
  source->AddBoolean("isJellyEnabled", features::IsOobeJellyEnabled());
  source->AddBoolean("isOobeJellyEnabled", features::IsOobeJellyEnabled());
  source->AddBoolean("isOobeJellyModalEnabled",
                     features::IsOobeJellyModalEnabled());
  source->AddBoolean("isBootAnimationEnabled",
                     features::IsBootAnimationEnabled());
  source->AddBoolean("isOobeAssistantEnabled",
                     !features::IsOobeSkipAssistantEnabled());
  source->AddBoolean("isOobeGaiaInfoScreenEnabled",
                     features::IsOobeGaiaInfoScreenEnabled());
  source->AddBoolean("isChoobeEnabled", features::IsOobeChoobeEnabled());
  source->AddBoolean("isSoftwareUpdateEnabled",
                     features::IsOobeSoftwareUpdateEnabled());
  source->AddBoolean(
      "isArcVmDataMigrationEnabled",
      base::FeatureList::IsEnabled(arc::kEnableArcVmDataMigration));

  source->AddBoolean("isTouchpadScrollEnabled",
                     features::IsOobeTouchpadScrollEnabled());

  source->AddBoolean("isDrivePinningEnabled",
                     drive::util::IsOobeDrivePinningScreenEnabled());

  // Whether the timings in oobe_trace.js will be output to the console.
  source->AddBoolean(
      "printFrontendTimings",
      command_line->HasSwitch(switches::kOobePrintFrontendLoadTimings));

  source->AddBoolean("isDisplaySizeEnabled",
                     features::IsOobeDisplaySizeEnabled());

  source->AddBoolean("isPersonalizedOnboarding",
                     features::IsOobePersonalizedOnboardingEnabled());

  source->AddBoolean("isPerksDiscoveryEnabled",
                     features::IsOobePerksDiscoveryEnabled());

  source->AddBoolean("isOobeSoftwareUpdateEnabled",
                     features::IsOobeSoftwareUpdateEnabled());

  source->AddBoolean("isPasswordlessGaiaEnabledForConsumers",
                     features::IsPasswordlessGaiaEnabledForConsumers());

  source->AddBoolean("isRemoteActivityNotificationEnabled",
                     base::FeatureList::IsEnabled(
                         remoting::features::kEnableCrdAdminRemoteAccessV2));

  source->AddBoolean("isSplitModifierKeyboardInfoEnabled",
                     features::IsOobeSplitModifierKeyboardInfoEnabled());

  source->AddBoolean("isOobeAddUserDuringEnrollmentEnabled",
                     features::IsOobeAddUserDuringEnrollmentEnabled());

  // Configure shared resources
  AddProductLogoResources(source);
  if (ash::features::IsBootAnimationEnabled()) {
    AddBootAnimationResources(source);
  }

  quick_unlock::AddFingerprintResources(source);
  AddSyncConsentResources(source);
  AddArcScreensResources(source);
  AddAssistantScreensResources(source);
  AddMultiDeviceSetupResources(source);

  AddDebuggerResources(source);
  AddTestAPIResources(source);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc, "object-src chrome:;");

  // Only add a filter when runing as test.
  const bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                               command_line->HasSwitch(::switches::kTestType);
  if (is_running_test) {
    source->SetRequestFilter(::test::GetTestShouldHandleRequest(),
                             ::test::GetTestFilesRequestFilter());
  }
}

std::string GetDisplayType(const GURL& url) {
  std::string path = url.path().size() ? url.path().substr(1) : "";

  constexpr auto kKnownDisplayTypes = base::MakeFixedFlatSet<std::string_view>(
      {OobeUI::kAppLaunchSplashDisplay, OobeUI::kGaiaSigninDisplay,
       OobeUI::kOobeDisplay, OobeUI::kOobeTestLoader});

  if (!kKnownDisplayTypes.contains(path)) {
    NOTREACHED_IN_MIGRATION()
        << "Unknown display type '" << path << "'. Setting default.";
    return OobeUI::kOobeDisplay;
  }
  return path;
}

}  // namespace

struct DisplayScaleFactor {
  int longest_side;
  float scale_factor;
};

const DisplayScaleFactor k4KDisplay = {3840, 1.5f},
                         kMediumDisplay = {1440, 4.f / 3};

bool OobeUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                         command_line->HasSwitch(::switches::kTestType);

  return ash::ProfileHelper::IsSigninProfile(
             Profile::FromBrowserContext(browser_context)) ||
         is_running_test;
}

void OobeUI::ConfigureOobeDisplay() {
  network_state_informer_ = new NetworkStateInformer();
  network_state_informer_->Init();

  AddWebUIHandler(std::make_unique<NetworkDropdownHandler>());

  AddScreenHandler(std::make_unique<UpdateScreenHandler>());

  if (display_type_ == kOobeDisplay) {
    AddScreenHandler(std::make_unique<WelcomeScreenHandler>());

    AddScreenHandler(std::make_unique<DemoPreferencesScreenHandler>());
  }

  AddScreenHandler(std::make_unique<QuickStartScreenHandler>());

  AddScreenHandler(std::make_unique<NetworkScreenHandler>());

  AddScreenHandler(std::make_unique<EnableAdbSideloadingScreenHandler>());

  AddScreenHandler(std::make_unique<EnableDebuggingScreenHandler>());

  AddScreenHandler(std::make_unique<ResetScreenHandler>());

  AddScreenHandler(std::make_unique<WrongHWIDScreenHandler>());

  AddScreenHandler(std::make_unique<AutoEnrollmentCheckScreenHandler>());

  AddScreenHandler(std::make_unique<HIDDetectionScreenHandler>());

  AddScreenHandler(std::make_unique<ErrorScreenHandler>());

  error_screen_ =
      std::make_unique<ErrorScreen>(GetView<ErrorScreenHandler>()->AsWeakPtr());
  ErrorScreen* error_screen = error_screen_.get();

  AddScreenHandler(std::make_unique<EnrollmentScreenHandler>());

  AddScreenHandler(std::make_unique<LocaleSwitchScreenHandler>());

  AddScreenHandler(std::make_unique<TermsOfServiceScreenHandler>());

  AddScreenHandler(std::make_unique<SyncConsentScreenHandler>());

  if (base::FeatureList::IsEnabled(arc::kEnableArcVmDataMigration)) {
    AddScreenHandler(std::make_unique<ArcVmDataMigrationScreenHandler>());
  }

  AddScreenHandler(std::make_unique<RecommendAppsScreenHandler>());

  AddScreenHandler(std::make_unique<AppDownloadingScreenHandler>());

  if (features::IsOobeAiIntroEnabled()) {
    AddScreenHandler(std::make_unique<AiIntroScreenHandler>());
  }

  if (features::IsOobeGeminiIntroEnabled()) {
    AddScreenHandler(std::make_unique<GeminiIntroScreenHandler>());
  }

  AddScreenHandler(std::make_unique<DemoSetupScreenHandler>());

  AddScreenHandler(std::make_unique<FamilyLinkNoticeScreenHandler>());

  AddScreenHandler(std::make_unique<FingerprintSetupScreenHandler>());

  AddScreenHandler(std::make_unique<LocalPasswordSetupHandler>());
  AddScreenHandler(std::make_unique<PasswordSelectionScreenHandler>());
  AddScreenHandler(std::make_unique<ApplyOnlinePasswordScreenHandler>());

  AddScreenHandler(std::make_unique<LocalDataLossWarningScreenHandler>());
  AddScreenHandler(std::make_unique<EnterOldPasswordScreenHandler>());

  AddScreenHandler(std::make_unique<OSAuthErrorScreenHandler>());
  AddScreenHandler(std::make_unique<FactorSetupSuccessScreenHandler>());

  AddScreenHandler(std::make_unique<GestureNavigationScreenHandler>());

  AddScreenHandler(std::make_unique<MarketingOptInScreenHandler>());

  AddScreenHandler(std::make_unique<GaiaScreenHandler>(network_state_informer_,
                                                       error_screen));

  AddScreenHandler(std::make_unique<OnlineAuthenticationScreenHandler>());

  AddScreenHandler(std::make_unique<UserAllowlistCheckScreenHandler>());

  AddScreenHandler(std::make_unique<SamlConfirmPasswordHandler>());

  AddScreenHandler(std::make_unique<SignInFatalErrorScreenHandler>());

  AddScreenHandler(std::make_unique<OfflineLoginScreenHandler>());

  AddWebUIHandler(std::make_unique<SshConfiguredHandler>());

  AddScreenHandler(std::make_unique<AppLaunchSplashScreenHandler>());

  AddScreenHandler(std::make_unique<DeviceDisabledScreenHandler>());

  AddScreenHandler(std::make_unique<EncryptionMigrationScreenHandler>());

  AddScreenHandler(std::make_unique<ManagementTransitionScreenHandler>());

  AddScreenHandler(std::make_unique<UpdateRequiredScreenHandler>());

  AddScreenHandler(
      std::make_unique<AssistantOptInFlowScreenHandler>(/*is_oobe=*/true));

  AddScreenHandler(std::make_unique<MultiDeviceSetupScreenHandler>());

  AddScreenHandler(std::make_unique<PackagedLicenseScreenHandler>());

  AddScreenHandler(std::make_unique<UserCreationScreenHandler>());

  AddScreenHandler(std::make_unique<TpmErrorScreenHandler>());
  AddScreenHandler(std::make_unique<InstallAttributesErrorScreenHandler>());

  AddScreenHandler(std::make_unique<ParentalHandoffScreenHandler>());

  if (switches::IsOsInstallAllowed()) {
    AddScreenHandler(std::make_unique<OsInstallScreenHandler>());
    AddScreenHandler(std::make_unique<OsTrialScreenHandler>());
  }

  AddScreenHandler(std::make_unique<HWDataCollectionScreenHandler>());

  AddScreenHandler(std::make_unique<ConsolidatedConsentScreenHandler>());

  AddScreenHandler(std::make_unique<CryptohomeRecoverySetupScreenHandler>());

  AddScreenHandler(std::make_unique<GuestTosScreenHandler>());

  AddScreenHandler(std::make_unique<SmartPrivacyProtectionScreenHandler>());

  AddScreenHandler(std::make_unique<ThemeSelectionScreenHandler>());

  if (features::IsOobeChoobeEnabled()) {
    AddScreenHandler(std::make_unique<ChoobeScreenHandler>());
  }

  if (features::IsOobeSoftwareUpdateEnabled()) {
    AddScreenHandler(std::make_unique<ConsumerUpdateScreenHandler>());
  }

  if (features::IsOobeTouchpadScrollEnabled()) {
    AddScreenHandler(std::make_unique<TouchpadScrollScreenHandler>());
  }

  if (features::IsOobeGaiaInfoScreenEnabled()) {
    AddScreenHandler(std::make_unique<GaiaInfoScreenHandler>());
  }

  if (features::IsOobeDisplaySizeEnabled()) {
    AddScreenHandler(std::make_unique<DisplaySizeScreenHandler>());
  }

  AddScreenHandler(std::make_unique<CategoriesSelectionScreenHandler>());
  AddScreenHandler(std::make_unique<PersonalizedRecommendAppsScreenHandler>());

  AddScreenHandler(std::make_unique<AddChildScreenHandler>());

  if (drive::util::IsOobeDrivePinningScreenEnabled()) {
    AddScreenHandler(std::make_unique<DrivePinningScreenHandler>());
  }

  AddScreenHandler(std::make_unique<PerksDiscoveryScreenHandler>());

  AddScreenHandler(std::make_unique<LocalStateErrorScreenHandler>());

  AddScreenHandler(std::make_unique<CryptohomeRecoveryScreenHandler>());

  AddScreenHandler(std::make_unique<SplitModifierKeyboardInfoScreenHandler>());

  if (features::IsOobeAddUserDuringEnrollmentEnabled()) {
    AddScreenHandler(std::make_unique<AccountSelectionScreenHandler>());
  }

  if (base::FeatureList::IsEnabled(
          remoting::features::kEnableCrdAdminRemoteAccessV2)) {
    AddScreenHandler(
        std::make_unique<RemoteActivityNotificationScreenHandler>());
  }

  Profile* const profile = Profile::FromWebUI(web_ui());
  // Set up the chrome://theme/ source, for Chrome logo.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  // Set up the chrome://terms/ data source, for EULA content.
  content::URLDataSource::Add(
      profile,
      std::make_unique<AboutUIHTMLSource>(chrome::kChromeUITermsHost, profile));

  content::WebContents* contents = web_ui()->GetWebContents();

  // TabHelper is required for OOBE webui to make webview working on it.
  extensions::TabHelper::CreateForWebContents(contents);

  if (ShouldUpScaleOobe()) {
    UpScaleOobe();
  }

  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    oobe_display_chooser_ = std::make_unique<OobeDisplayChooser>();
  }
}

bool OobeUI::ShouldUpScaleOobe() {
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  return upscaled_display_id_ != display_id && switches::ShouldScaleOobe() &&
         policy::EnrollmentRequisitionManager::IsMeetDevice();
}

void OobeUI::UpScaleOobe() {
  upscaled_display_id_ = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const gfx::Size size =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area_size();
  const int longest_side = std::max(size.width(), size.height());
  if (longest_side >= k4KDisplay.longest_side) {
    display_manager->UpdateZoomFactor(upscaled_display_id_,
                                      k4KDisplay.scale_factor);
  } else if (longest_side >= kMediumDisplay.longest_side) {
    display_manager->UpdateZoomFactor(upscaled_display_id_,
                                      kMediumDisplay.scale_factor);
  }
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  multidevice_setup::MultiDeviceSetupService* service =
      multidevice_setup::MultiDeviceSetupServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (service) {
    service->BindMultiDeviceSetup(std::move(receiver));
  }
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<multidevice_setup::mojom::PrivilegedHostDeviceSetter>
        receiver) {
  multidevice_setup::MultiDeviceSetupService* service =
      multidevice_setup::MultiDeviceSetupServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (service) {
    service->BindPrivilegedHostDeviceSetter(std::move(receiver));
  }
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver) {
  GetESimManager(std::move(receiver));
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<screens_factory::mojom::ScreensFactory> receiver) {
  oobe_screens_handler_factory_ =
      std::make_unique<OobeScreensHandlerFactory>(std::move(receiver));
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<auth::mojom::AuthFactorConfig> receiver) {
  auth::BindToAuthFactorConfig(std::move(receiver),
                               quick_unlock::QuickUnlockFactory::GetDelegate(),
                               g_browser_process->local_state());
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<auth::mojom::PinFactorEditor> receiver) {
  auto* pin_backend = quick_unlock::PinBackend::GetInstance();
  CHECK(pin_backend);
  auth::BindToPinFactorEditor(std::move(receiver),
                              quick_unlock::QuickUnlockFactory::GetDelegate(),
                              g_browser_process->local_state(), *pin_backend);
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<auth::mojom::PasswordFactorEditor> receiver) {
  auth::BindToPasswordFactorEditor(
      std::move(receiver), quick_unlock::QuickUnlockFactory::GetDelegate(),
      g_browser_process->local_state());
}

OobeUI::OobeUI(content::WebUI* web_ui, const GURL& url)
    : ui::MojoWebUIController(web_ui, true /* enable_chrome_send */) {
  LOG(WARNING) << "OobeUI created";
  display_type_ = GetDisplayType(url);

  auto core_oobe_handler = std::make_unique<CoreOobeHandler>();
  core_handler_ = core_oobe_handler.get();
  core_oobe_ =
      std::make_unique<CoreOobe>(display_type_, core_oobe_handler->AsWeakPtr());
  web_ui->AddMessageHandler(std::move(core_oobe_handler));

  ConfigureOobeDisplay();

  AddScreenHandler(std::make_unique<PinSetupScreenHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enable_debugger = command_line->HasSwitch(switches::kShowOobeDevOverlay);
  if (enable_debugger) {
    base::SysInfo::CrashIfChromeOSNonTestImage();
    AddWebUIHandler(std::make_unique<DebugOverlayHandler>());
  }

  bool enable_test_api = command_line->HasSwitch(switches::kEnableOobeTestAPI);
  if (enable_test_api) {
    AddWebUIHandler(std::make_unique<OobeTestAPIHandler>());
  }

  base::Value::Dict localized_strings = GetLocalizedStrings();

  // Set up the chrome://oobe/ source.
  CreateAndAddOobeUIDataSource(Profile::FromWebUI(web_ui), localized_strings,
                               display_type_);
}

OobeUI::~OobeUI() {
  for (Observer& observer : observer_list_) {
    observer.OnDestroyingOobeUI();
  }
  LOG(WARNING) << "OobeUI destroyed";
}

// static
void OobeUI::AddOobeComponents(content::WebUIDataSource* source) {
  // Add all resources from OOBE's autogenerated GRD.
  constexpr auto kConditionalResources =
      base::MakeFixedFlatSet<std::string_view>({
          "debug/debug.js",
          "debug/no_debug.js",
          "debug/quick_start_debugger.js",
          "debug/quick_start_debugger.html.js",
          "components/oobe_vars/oobe_custom_vars.css.js",
          "components/oobe_vars/oobe_custom_vars_remora.css.js",
          "test_api/no_test_api.js",
          "test_api/test_api.js",
      });
  for (const auto& path : base::make_span(kOobeResources, kOobeResourcesSize)) {
    if (!kConditionalResources.contains(path.path)) {
      source->AddResourcePath(path.path, path.id);
    }
  }
  // Add Gaia Authenticator resources
  source->AddResourcePaths(
      base::make_span(kGaiaAuthHostResources, kGaiaAuthHostResourcesSize));

  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    source->AddResourcePath(
        kOobeCustomVarsCssJs,
        IDR_OOBE_COMPONENTS_OOBE_VARS_OOBE_CUSTOM_VARS_REMORA_CSS_JS);
  } else {
    source->AddResourcePath(
        kOobeCustomVarsCssJs,
        IDR_OOBE_COMPONENTS_OOBE_VARS_OOBE_CUSTOM_VARS_CSS_JS);
  }

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");
}

CoreOobe* OobeUI::GetCoreOobe() {
  return core_oobe_.get();
}

ErrorScreen* OobeUI::GetErrorScreen() {
  return error_screen_.get();
}

OobeScreensHandlerFactory* OobeUI::GetOobeScreensHandlerFactory() {
  return oobe_screens_handler_factory_.get();
}

base::Value::Dict OobeUI::GetLocalizedStrings() {
  base::Value::Dict localized_strings;
  core_handler_->GetLocalizedStrings(&localized_strings);
  for (BaseWebUIHandler* handler : webui_handlers_) {
    handler->GetLocalizedStrings(&localized_strings);
  }

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);
  localized_strings.Set("app_locale", app_locale);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  localized_strings.Set("buildType", "chrome");
#else
  localized_strings.Set("buildType", "chromium");
#endif

  std::string oobeClasses = "";
  // TODO (b/268463435) Cleanup OobeJelly
  if (features::IsOobeJellyEnabled()) {
    oobeClasses += "jelly-enabled ";
  }
  if (features::IsOobeJellyModalEnabled()) {
    oobeClasses += "jelly-modal-enabled ";
  }
  if (features::IsBootAnimationEnabled()) {
    oobeClasses += "boot-animation-enabled ";
  }
  localized_strings.Set("oobeClasses", oobeClasses);

  bool keyboard_driven_oobe = ash::system::InputDeviceSettings::Get()
                                  ->ForceKeyboardDrivenUINavigation();
  localized_strings.Set("highlightStrength",
                        keyboard_driven_oobe ? "strong" : "normal");

  localized_strings.Set(
      "changePictureVideoModeEnabled",
      base::FeatureList::IsEnabled(::features::kChangePictureVideoMode));
  return localized_strings;
}

void OobeUI::AddWebUIHandler(std::unique_ptr<BaseWebUIHandler> handler) {
  webui_handlers_.push_back(handler.get());
  webui_only_handlers_.push_back(handler.get());
  web_ui()->AddMessageHandler(std::move(handler));
}

void OobeUI::AddScreenHandler(std::unique_ptr<BaseScreenHandler> handler) {
  webui_handlers_.push_back(handler.get());
  screen_handlers_.push_back(handler.get());
  web_ui()->AddMessageHandler(std::move(handler));
}

void OobeUI::InitializeHandlers() {
  for (BaseWebUIHandler* handler : webui_only_handlers_) {
    CHECK(!handler->IsJavascriptAllowed());
    handler->AllowJavascript();
  }

  for (BaseScreenHandler* handler : screen_handlers_) {
    CHECK(!handler->IsJavascriptAllowed());
    handler->AllowJavascript();
  }

  // Notify listeners that JS is allowed and ready.
  ready_ = true;
  ready_callbacks_.Notify();
}

void OobeUI::CurrentScreenChanged(OobeScreenId new_screen) {
  previous_screen_ = current_screen_;

  current_screen_ = new_screen;
  for (Observer& observer : observer_list_) {
    observer.OnCurrentScreenChanged(previous_screen_, new_screen);
  }
}

void OobeUI::OnBackdropLoaded() {
  for (Observer& observer : observer_list_) {
    observer.OnBackdropLoaded();
  }
}

bool OobeUI::IsJSReady(base::OnceClosure display_is_ready_callback) {
  if (!ready_) {
    ready_callbacks_.AddUnsafe(std::move(display_is_ready_callback));
    return ready_;
  }
  std::move(display_is_ready_callback).Run();
  return ready_;
}

gfx::NativeView OobeUI::GetNativeView() {
  return web_ui()->GetWebContents()->GetNativeView();
}

gfx::NativeWindow OobeUI::GetTopLevelNativeWindow() {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

gfx::Size OobeUI::GetViewSize() {
  return web_ui()->GetWebContents()->GetSize();
}

void OobeUI::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OobeUI::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OobeUI::OnDisplayConfigurationChanged() {
  if (oobe_display_chooser_) {
    oobe_display_chooser_->TryToPlaceUiOnTouchDisplay();
  }
  if (ShouldUpScaleOobe()) {
    UpScaleOobe();
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(OobeUI)

}  // namespace ash
