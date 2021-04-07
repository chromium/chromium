// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/esim_manager.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/active_directory_login_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/active_directory_password_change_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/debug/debug_overlay_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/multidevice_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"
#include "chrome/browser/ui/webui/chromeos/login/packaged_license_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/parental_handoff_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/ssh_configured_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/testapi/oobe_test_api_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/user_image_source.h"
#include "chrome/browser/ui/webui/chromeos/video_source.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/test_files_request_filter.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/oobe_resources.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/display/display.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"

namespace chromeos {

namespace {

const char* kKnownDisplayTypes[] = {
    OobeUI::kAppLaunchSplashDisplay, OobeUI::kGaiaSigninDisplay,
    OobeUI::kLoginDisplay, OobeUI::kOobeDisplay};

// Sorted
constexpr char kArcAssistantLogoPath[] = "assistant_logo.png";
constexpr char kArcOverlayCSSPath[] = "overlay.css";
constexpr char kArcPlaystoreCSSPath[] = "playstore.css";
constexpr char kArcPlaystoreJSPath[] = "playstore.js";
constexpr char kArcPlaystoreLogoPath[] = "playstore.svg";
constexpr char kArcSupervisionIconPath[] = "supervision_icon.png";
constexpr char kCustomElementsHTMLPath[] = "custom_elements.html";
constexpr char kCustomElementsJSPath[] = "custom_elements.js";
constexpr char kDebuggerJSPath[] = "debug.js";
constexpr char kKeyboardUtilsJSPath[] = "keyboard_utils.js";
constexpr char kLoginJSPath[] = "login.js";
constexpr char kOobeJSPath[] = "oobe.js";
constexpr char kProductLogoPath[] = "product-logo.png";
constexpr char kRecommendAppListViewJSPath[] = "recommend_app_list_view.js";
constexpr char kTestAPIJSPath[] = "test_api.js";
constexpr char kWebviewSamlInjectedJSPath[] = "webview_saml_injected.js";

// Public
constexpr char kLoginScreenBehaviorHTML[] = "components/login_screen_behavior.html";
constexpr char kLoginScreenBehaviorJS[] = "components/login_screen_behavior.js";
constexpr char kMultiStepBehaviorHTML[] = "components/multi_step_behavior.html";
constexpr char kMultiStepBehaviorJS[] = "components/multi_step_behavior.js";

// Components
constexpr char kOobeSharedVarsCssHTML[] =
    "components/oobe_shared_vars_css.html";
constexpr char kOobeCustomVarsCssHTML[] =
    "components/oobe_custom_vars_css.html";
constexpr char kCommonStylesHTML[] = "components/common_styles.html";
constexpr char kI18nBehaviorHTML[] = "components/oobe_i18n_behavior.html";
constexpr char kI18nBehaviorJS[] = "components/oobe_i18n_behavior.js";
constexpr char kI18nSetupHTML[] = "components/i18n_setup.html";
constexpr char kDialogHostBehaviorHTML[] =
    "components/oobe_dialog_host_behavior.html";
constexpr char kDialogHostBehaviorJS[] =
    "components/oobe_dialog_host_behavior.js";
constexpr char kFocusBehaviorHTML[] = "components/oobe_focus_behavior.html";
constexpr char kFocusBehaviorJS[] = "components/oobe_focus_behavior.js";
constexpr char kScrollableBehaviorHTML[] =
    "components/oobe_scrollable_behavior.html";
constexpr char kScrollableBehaviorJS[] =
    "components/oobe_scrollable_behavior.js";
constexpr char kHDIronIconHTML[] = "components/hd_iron_icon.html";
constexpr char kHDIronIconJS[] = "components/hd_iron_icon.js";
constexpr char kOobeAdaptiveDialogHTML[] =
    "components/oobe_adaptive_dialog.html";
constexpr char kOobeAdaptvieDialogJS[] = "components/oobe_adaptive_dialog.js";
constexpr char kOobeContentDialogHTML[] = "components/oobe_content_dialog.html";
constexpr char kOobeContentDialogJS[] = "components/oobe_content_dialog.js";
constexpr char kOobeDialogHTML[] = "components/oobe_dialog.html";
constexpr char kOobeDialogJS[] = "components/oobe_dialog.js";
constexpr char kOobeLoadingDialogHTML[] = "components/oobe_loading_dialog.html";
constexpr char kOobeLoadingDialogJS[] = "components/oobe_loading_dialog.js";
constexpr char kOobeCarouselHTML[] = "components/oobe_carousel.html";
constexpr char kOobeCarouselJS[] = "components/oobe_carousel.js";
constexpr char kOobeSlideHTML[] = "components/oobe_slide.html";
constexpr char kOobeSlideJS[] = "components/oobe_slide.js";
constexpr char kProgressListItemHTML[] = "components/progress_list_item.html";
constexpr char kProgressListItemJS[] = "components/progress_list_item.js";
constexpr char kThrobberNoticeHTML[] = "components/throbber_notice.html";
constexpr char kThrobberNoticeJS[] = "components/throbber_notice.js";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kLogo24PX1XSvgPath[] = "logo_24px-1x.svg";
constexpr char kLogo24PX2XSvgPath[] = "logo_24px-2x.svg";
constexpr char kSyncConsentIcons[] = "sync-consent-icons.html";
constexpr char kArcAppDownloadingVideoPath[] = "res/arc_app_dowsnloading.mp4";
#endif

// Adds various product logo resources.
void AddProductLogoResources(content::WebUIDataSource* source) {
  // Required for Assistant OOBE.
  source->AddResourcePath(kArcAssistantLogoPath, IDR_ASSISTANT_LOGO_PNG);
  source->AddResourcePath(kArcSupervisionIconPath, IDR_SUPERVISION_ICON_PNG);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath(kLogo24PX1XSvgPath, IDR_PRODUCT_LOGO_24PX_1X);
  source->AddResourcePath(kLogo24PX2XSvgPath, IDR_PRODUCT_LOGO_24PX_2X);
#endif

  // Required in encryption migration screen.
  source->AddResourcePath(kProductLogoPath, IDR_PRODUCT_LOGO_64);
}

void AddSyncConsentResources(content::WebUIDataSource* source) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath(kSyncConsentIcons,
                          IDR_PRODUCT_CHROMEOS_SYNC_CONSENT_SCREEN_ICONS);
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

  source->AddResourcePath(kRecommendAppListViewJSPath,
                          IDR_ARC_SUPPORT_RECOMMEND_APP_LIST_VIEW_JS);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath(kArcAppDownloadingVideoPath,
                          IDR_OOBE_ARC_APPS_DOWNLOADING_VIDEO);
#endif
}

void AddAssistantScreensResources(content::WebUIDataSource* source) {
  source->AddResourcePath("voice_match_animation.json",
                          IDR_ASSISTANT_VOICE_MATCH_ANIMATION);
  source->AddResourcePath("voice_match_already_setup_animation.json",
                          IDR_ASSISTANT_VOICE_MATCH_ALREADY_SETUP_ANIMATION);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");
}

void AddGestureNavigationResources(content::WebUIDataSource* source) {
  source->AddResourcePath("gesture_go_home.json",
                          IDR_GESTURE_NAVIGATION_GO_HOME_ANIMATION);
  source->AddResourcePath("gesture_go_back.json",
                          IDR_GESTURE_NAVIGATION_GO_BACK_ANIMATION);
  source->AddResourcePath("gesture_hotseat_overview.json",
                          IDR_GESTURE_NAVIGATION_HOTSEAT_OVERVIEW_ANIMATION);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");
}

void AddMarketingOptInResources(content::WebUIDataSource* source) {
  source->AddResourcePath("all_set.json",
                          IDR_MARKETING_OPT_IN_ALL_SET_ANIMATION);
  source->AddResourcePath("all_set_new_noloop.json",
                          IDR_MARKETING_OPT_IN_ALL_SET_ANIMATION_NEW_NOLOOP);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");
}

void AddMultiDeviceSetupResources(content::WebUIDataSource* source) {
  source->AddResourcePath("multidevice_setup.json",
                          IDR_MULTIDEVICE_SETUP_ANIMATION);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");
}

void AddAppDownloadingResources(content::WebUIDataSource* source) {
  source->AddResourcePath("downloading_apps.json",
                          IDR_APPS_DOWNLOADING_ANIMATION);
}

void AddDebuggerResources(content::WebUIDataSource* source) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enable_debugger =
      command_line->HasSwitch(::chromeos::switches::kShowOobeDevOverlay);
  // Enable for ChromeOS-on-linux for developers and test images.
  if (enable_debugger && base::SysInfo::IsRunningOnChromeOS()) {
    LOG(WARNING) << "OOBE Debug overlay can only be used on test images";
    base::SysInfo::CrashIfChromeOSNonTestImage();
  }
  if (enable_debugger) {
    source->AddResourcePath(kDebuggerJSPath, IDR_OOBE_DEBUGGER_JS);
  } else {
    source->AddResourcePath(kDebuggerJSPath, IDR_OOBE_DEBUGGER_STUB_JS);
  }
}

void AddTestAPIResources(content::WebUIDataSource* source) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enable_test_api =
      command_line->HasSwitch(::chromeos::switches::kEnableOobeTestAPI);
  if (enable_test_api) {
    source->AddResourcePath(kTestAPIJSPath, IDR_OOBE_TEST_API_JS);
  } else {
    source->AddResourcePath(kTestAPIJSPath, IDR_OOBE_TEST_API_STUB_JS);
  }
}

// Default and non-shared resource definition for kOobeDisplay display type.
// chrome://oobe/oobe
void AddOobeDisplayTypeDefaultResources(content::WebUIDataSource* source) {
  source->SetDefaultResource(IDR_OOBE_HTML);
  source->AddResourcePath(kOobeJSPath, IDR_OOBE_JS);
  source->AddResourcePath(kCustomElementsHTMLPath,
                          IDR_CUSTOM_ELEMENTS_OOBE_HTML);
  source->AddResourcePath(kCustomElementsJSPath, IDR_CUSTOM_ELEMENTS_OOBE_JS);
}

// Default and non-shared resource definition for kLoginDisplay display type.
// chrome://oobe/login
void AddLoginDisplayTypeDefaultResources(content::WebUIDataSource* source) {
  source->SetDefaultResource(IDR_MD_LOGIN_HTML);
  source->AddResourcePath(kLoginJSPath, IDR_MD_LOGIN_JS);
  source->AddResourcePath(kCustomElementsHTMLPath,
                          IDR_CUSTOM_ELEMENTS_LOGIN_HTML);
  source->AddResourcePath(kCustomElementsJSPath, IDR_CUSTOM_ELEMENTS_LOGIN_JS);
}

// Creates a WebUIDataSource for chrome://oobe
content::WebUIDataSource* CreateOobeUIDataSource(
    const base::DictionaryValue& localized_strings,
    const std::string& display_type) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIOobeHost);
  source->AddLocalizedStrings(localized_strings);
  source->UseStringsJs();

  OobeUI::AddOobeComponents(source, localized_strings);

  // First, configure default and non-shared resources for the current display
  // type.
  if (display_type == OobeUI::kOobeDisplay) {
    AddOobeDisplayTypeDefaultResources(source);
  } else if (display_type == OobeUI::kLockDisplay) {
    NOTREACHED();
  } else {
    AddLoginDisplayTypeDefaultResources(source);
  }

  // Configure shared resources
  AddProductLogoResources(source);

  chromeos::quick_unlock::AddFingerprintResources(source);
  AddSyncConsentResources(source);
  AddArcScreensResources(source);
  AddAssistantScreensResources(source);
  AddGestureNavigationResources(source);
  AddMarketingOptInResources(source);
  AddMultiDeviceSetupResources(source);
  AddAppDownloadingResources(source);

  AddDebuggerResources(source);
  AddTestAPIResources(source);

  source->AddResourcePath(kWebviewSamlInjectedJSPath,
                          IDR_GAIA_AUTH_WEBVIEW_SAML_INJECTED_JS);
  source->AddResourcePath(kKeyboardUtilsJSPath, IDR_KEYBOARD_UTILS_JS);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc, "object-src chrome:;");
  source->DisableTrustedTypesCSP();

  // Only add a filter when runing as test.
  const bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                               command_line->HasSwitch(::switches::kTestType);
  if (is_running_test)
    source->SetRequestFilter(::test::GetTestShouldHandleRequest(),
                             ::test::GetTestFilesRequestFilter());

  return source;
}

std::string GetDisplayType(const GURL& url) {
  std::string path = url.path().size() ? url.path().substr(1) : "";

  if (!base::Contains(kKnownDisplayTypes, path)) {
    LOG(ERROR) << "Unknown display type '" << path << "'. Setting default.";
    return OobeUI::kLoginDisplay;
  }
  return path;
}

}  // namespace

// static
const char OobeUI::kAppLaunchSplashDisplay[] = "app-launch-splash";
const char OobeUI::kGaiaSigninDisplay[] = "gaia-signin";
const char OobeUI::kLockDisplay[] = "lock";
const char OobeUI::kLoginDisplay[] = "login";
const char OobeUI::kOobeDisplay[] = "oobe";

void OobeUI::ConfigureOobeDisplay() {
  network_state_informer_ = new NetworkStateInformer();
  network_state_informer_->Init();

  AddWebUIHandler(
      std::make_unique<NetworkDropdownHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<UpdateScreenHandler>(js_calls_container_.get()));

  if (display_type_ == kOobeDisplay) {
    AddScreenHandler(std::make_unique<WelcomeScreenHandler>(
        js_calls_container_.get(), core_handler_));

    AddScreenHandler(std::make_unique<DemoPreferencesScreenHandler>(
        js_calls_container_.get()));
  }

  AddScreenHandler(std::make_unique<NetworkScreenHandler>(
      js_calls_container_.get(), core_handler_));

  AddScreenHandler(std::make_unique<EnableAdbSideloadingScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<EnableDebuggingScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<EulaScreenHandler>(
      js_calls_container_.get(), core_handler_));

  AddScreenHandler(
      std::make_unique<ResetScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<KioskAutolaunchScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<KioskEnableScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<WrongHWIDScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<AutoEnrollmentCheckScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<HIDDetectionScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<ErrorScreenHandler>(js_calls_container_.get()));

  error_screen_ = std::make_unique<ErrorScreen>(GetView<ErrorScreenHandler>());
  ErrorScreen* error_screen = error_screen_.get();

  AddScreenHandler(std::make_unique<EnrollmentScreenHandler>(
      js_calls_container_.get(), network_state_informer_, error_screen));

  AddScreenHandler(std::make_unique<LocaleSwitchScreenHandler>(
      js_calls_container_.get(), core_handler_));

  AddScreenHandler(
      std::make_unique<TermsOfServiceScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<SyncConsentScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<ArcTermsOfServiceScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<RecommendAppsScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<AppDownloadingScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<DemoSetupScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<FamilyLinkNoticeScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<FingerprintSetupScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<GestureNavigationScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<MarketingOptInScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<GaiaPasswordChangedScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<ActiveDirectoryLoginScreenHandler>(
      js_calls_container_.get(), core_handler_));

  auto password_change_handler =
      std::make_unique<ActiveDirectoryPasswordChangeScreenHandler>(
          js_calls_container_.get());

  AddScreenHandler(std::make_unique<GaiaScreenHandler>(
      js_calls_container_.get(), core_handler_, network_state_informer_));

  AddScreenHandler(std::make_unique<SignInFatalErrorScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<OfflineLoginScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::move(password_change_handler));

  auto signin_screen_handler = std::make_unique<SigninScreenHandler>(
      js_calls_container_.get(), network_state_informer_, error_screen,
      core_handler_, GetHandler<GaiaScreenHandler>());
  signin_screen_handler_ = signin_screen_handler.get();
  AddWebUIHandler(std::move(signin_screen_handler));

  AddWebUIHandler(
      std::make_unique<SshConfiguredHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<AppLaunchSplashScreenHandler>(
      js_calls_container_.get(), network_state_informer_, error_screen));

  AddScreenHandler(
      std::make_unique<DeviceDisabledScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<EncryptionMigrationScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<SupervisionTransitionScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<UpdateRequiredScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<AssistantOptInFlowScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<MultiDeviceSetupScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<PackagedLicenseScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<UserCreationScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<TpmErrorScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<ParentalHandoffScreenHandler>(
      js_calls_container_.get()));

  Profile* profile = Profile::FromWebUI(web_ui());
  // Set up the chrome://theme/ source, for Chrome logo.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  // Set up the chrome://terms/ data source, for EULA content.
  content::URLDataSource::Add(
      profile,
      std::make_unique<AboutUIHTMLSource>(chrome::kChromeUITermsHost, profile));

  // Set up the chrome://userimage/ source.
  content::URLDataSource::Add(profile, std::make_unique<UserImageSource>());

  // TabHelper is required for OOBE webui to make webview working on it.
  content::WebContents* contents = web_ui()->GetWebContents();
  extensions::TabHelper::CreateForWebContents(contents);

  // // Handler for the oobe video assets which will be shown if available.
  content::URLDataSource::Add(profile,
                              std::make_unique<chromeos::VideoSource>());

  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition())
    oobe_display_chooser_ = std::make_unique<OobeDisplayChooser>();
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  multidevice_setup::MultiDeviceSetupService* service =
      multidevice_setup::MultiDeviceSetupServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (service)
    service->BindMultiDeviceSetup(std::move(receiver));
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<multidevice_setup::mojom::PrivilegedHostDeviceSetter>
        receiver) {
  multidevice_setup::MultiDeviceSetupService* service =
      multidevice_setup::MultiDeviceSetupServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (service)
    service->BindPrivilegedHostDeviceSetter(std::move(receiver));
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void OobeUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver) {
  ash::GetESimManager(std::move(receiver));
}

OobeUI::OobeUI(content::WebUI* web_ui, const GURL& url)
    : ui::MojoWebUIController(web_ui, true /* enable_chrome_send */) {
  display_type_ = GetDisplayType(url);

  js_calls_container_ = std::make_unique<JSCallsContainer>();

  auto core_handler =
      std::make_unique<CoreOobeHandler>(js_calls_container_.get());
  core_handler_ = core_handler.get();

  AddWebUIHandler(std::move(core_handler));

  ConfigureOobeDisplay();

  AddScreenHandler(
      std::make_unique<PinSetupScreenHandler>(js_calls_container_.get()));
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enable_debugger =
      command_line->HasSwitch(::chromeos::switches::kShowOobeDevOverlay);
  // TODO(crbug.com/1073095): Also enable for ChromeOS test images.
  // Enable for ChromeOS-on-linux for developers.
  bool test_mode = !base::SysInfo::IsRunningOnChromeOS();

  if (enable_debugger && test_mode) {
    AddWebUIHandler(
        std::make_unique<DebugOverlayHandler>(js_calls_container_.get()));
  }

  bool enable_test_api =
      command_line->HasSwitch(::chromeos::switches::kEnableOobeTestAPI);
  if (enable_test_api) {
    AddWebUIHandler(
        std::make_unique<OobeTestAPIHandler>(js_calls_container_.get()));
  }

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  // Set up the chrome://oobe/ source.
  content::WebUIDataSource* html_source =
      CreateOobeUIDataSource(localized_strings, display_type_);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

OobeUI::~OobeUI() {
  for (Observer& observer : observer_list_)
    observer.OnDestroyingOobeUI();
  VLOG(4) << "~OobeUI";
}

// static

void OobeUI::AddOobeComponents(content::WebUIDataSource* source,
                               const base::DictionaryValue& localized_strings) {
  source->AddResourcePath(kLoginScreenBehaviorHTML,
                          IDR_OOBE_COMPONENTS_LOGIN_SCREEN_BEHAVIOR_HTML);
  source->AddResourcePath(kLoginScreenBehaviorJS,
                          IDR_OOBE_COMPONENTS_LOGIN_SCREEN_BEHAVIOR_JS);
  source->AddResourcePath(kMultiStepBehaviorHTML,
                          IDR_OOBE_COMPONENTS_MULTI_STEP_BEHAVIOR_HTML);
  source->AddResourcePath(kMultiStepBehaviorJS,
                          IDR_OOBE_COMPONENTS_MULTI_STEP_BEHAVIOR_JS);

  source->AddResourcePath(kI18nBehaviorHTML,
                          IDR_OOBE_COMPONENTS_I18N_BEHAVIOR_HTML);
  source->AddResourcePath(kI18nBehaviorJS,
                          IDR_OOBE_COMPONENTS_I18N_BEHAVIOR_JS);
  source->AddResourcePath(kI18nSetupHTML, IDR_OOBE_COMPONENTS_I18N_SETUP_HTML);
  source->AddResourcePath(kDialogHostBehaviorHTML,
                          IDR_OOBE_COMPONENTS_DIALOG_HOST_BEHAVIOR_HTML);
  source->AddResourcePath(kDialogHostBehaviorJS,
                          IDR_OOBE_COMPONENTS_DIALOG_HOST_BEHAVIOR_JS);
  source->AddResourcePath(kFocusBehaviorHTML,
                          IDR_OOBE_COMPONENTS_FOCUS_BEHAVIOR_HTML);
  source->AddResourcePath(kFocusBehaviorJS,
                          IDR_OOBE_COMPONENTS_FOCUS_BEHAVIOR_JS);
  source->AddResourcePath(kScrollableBehaviorHTML,
                          IDR_OOBE_COMPONENTS_SCROLLABLE_BEHAVIOR_HTML);
  source->AddResourcePath(kScrollableBehaviorJS,
                          IDR_OOBE_COMPONENTS_SCROLLABLE_BEHAVIOR_JS);

  source->AddResourcePath(kCommonStylesHTML,
                          IDR_OOBE_COMPONENTS_COMMON_STYLES_HTML);
  source->AddResourcePath(kOobeSharedVarsCssHTML,
                          IDR_OOBE_COMPONENTS_OOBE_SHARED_VARS_CSS_HTML);

  source->AddResourcePath(kHDIronIconHTML,
                          IDR_OOBE_COMPONENTS_HD_IRON_ICON_HTML);
  source->AddResourcePath(kHDIronIconJS, IDR_OOBE_COMPONENTS_HD_IRON_ICON_JS);

  source->AddResourcePath(kOobeDialogHTML,
                          IDR_OOBE_COMPONENTS_OOBE_DIALOG_HTML);
  source->AddResourcePath(kOobeDialogJS, IDR_OOBE_COMPONENTS_OOBE_DIALOG_JS);
  source->AddResourcePath(kOobeLoadingDialogHTML,
                          IDR_OOBE_COMPONENTS_OOBE_LOADING_DIALOG_HTML);
  source->AddResourcePath(kOobeLoadingDialogJS,
                          IDR_OOBE_COMPONENTS_OOBE_LOADING_DIALOG_JS);

  if (features::IsNewOobeLayoutEnabled()) {
    if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
      source->AddResourcePath(
          kOobeCustomVarsCssHTML,
          IDR_OOBE_COMPONENTS_OOBE_CUSTOM_VARS_REMORA_CSS_HTML);
    } else {
      source->AddResourcePath(kOobeCustomVarsCssHTML,
                              IDR_OOBE_COMPONENTS_OOBE_CUSTOM_VARS_CSS_HTML);
    }
  } else {
    source->AddResourcePath(kOobeCustomVarsCssHTML,
                            IDR_OOBE_COMPONENTS_OOBE_CUSTOM_VARS_OLD_CSS_HTML);
  }

  if (features::IsNewOobeLayoutEnabled()) {
    source->AddResourcePath(kOobeAdaptiveDialogHTML,
                            IDR_OOBE_COMPONENTS_OOBE_ADAPTIVE_DIALOG_HTML);
    source->AddResourcePath(kOobeAdaptvieDialogJS,
                            IDR_OOBE_COMPONENTS_OOBE_ADAPTIVE_DIALOG_JS);
    source->AddResourcePath(kOobeContentDialogHTML,
                            IDR_OOBE_COMPONENTS_OOBE_CONTENT_DIALOG_HTML);
    source->AddResourcePath(kOobeContentDialogJS,
                            IDR_OOBE_COMPONENTS_OOBE_CONTENT_DIALOG_JS);

    source->AddResourcePath("welcome_screen_animation.json",
                            IDR_LOGIN_WELCOME_SCREEN_ANIMATION);
    source->AddResourcePath("spinner.json", IDR_LOGIN_SPINNER_ANIMATION);
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::WorkerSrc,
        "worker-src blob: 'self';");

  } else {
    source->AddResourcePath(kOobeAdaptiveDialogHTML,
                            IDR_OOBE_COMPONENTS_OOBE_ADAPTIVE_DIALOG_OLD_HTML);
    source->AddResourcePath(kOobeAdaptvieDialogJS,
                            IDR_OOBE_COMPONENTS_OOBE_ADAPTIVE_DIALOG_OLD_JS);
    source->AddResourcePath(kOobeContentDialogHTML,
                            IDR_OOBE_COMPONENTS_OOBE_CONTENT_DIALOG_OLD_HTML);
    source->AddResourcePath(kOobeContentDialogJS,
                            IDR_OOBE_COMPONENTS_OOBE_CONTENT_DIALOG_OLD_JS);
  }

  source->AddResourcePath(kOobeCarouselHTML,
                          IDR_OOBE_COMPONENTS_OOBE_CAROUSEL_HTML);
  source->AddResourcePath(kOobeCarouselJS,
                          IDR_OOBE_COMPONENTS_OOBE_CAROUSEL_JS);
  source->AddResourcePath(kOobeSlideHTML, IDR_OOBE_COMPONENTS_OOBE_SLIDE_HTML);
  source->AddResourcePath(kOobeSlideJS, IDR_OOBE_COMPONENTS_OOBE_SLIDE_JS);

  source->AddResourcePath(kProgressListItemHTML,
                          IDR_OOBE_COMPONENTS_PROGRESS_LIST_ITEM_HTML);
  source->AddResourcePath(kProgressListItemJS,
                          IDR_OOBE_COMPONENTS_PROGRESS_LIST_ITEM_JS);

  source->AddResourcePath(kThrobberNoticeHTML,
                          IDR_OOBE_COMPONENTS_THROBBER_NOTICE_HTML);
  source->AddResourcePath(kThrobberNoticeJS,
                          IDR_OOBE_COMPONENTS_THROBBER_NOTICE_JS);
}

CoreOobeView* OobeUI::GetCoreOobeView() {
  return core_handler_;
}

ErrorScreen* OobeUI::GetErrorScreen() {
  return error_screen_.get();
}

void OobeUI::GetLocalizedStrings(base::DictionaryValue* localized_strings) {
  for (BaseWebUIHandler* handler : webui_handlers_)
    handler->GetLocalizedStrings(localized_strings);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings);
  localized_strings->SetString("app_locale", app_locale);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  localized_strings->SetString("buildType", "chrome");
#else
  localized_strings->SetString("buildType", "chromium");
#endif

  bool keyboard_driven_oobe =
      system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
  localized_strings->SetString("highlightStrength",
                               keyboard_driven_oobe ? "strong" : "normal");

  localized_strings->SetBoolean(
      "changePictureVideoModeEnabled",
      base::FeatureList::IsEnabled(::features::kChangePictureVideoMode));
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
  js_calls_container_->ExecuteDeferredJSCalls(web_ui());

  ready_ = true;
  ready_callbacks_.Notify();

  for (BaseWebUIHandler* handler : webui_only_handlers_)
    handler->InitializeBase();

  for (BaseScreenHandler* handler : screen_handlers_)
    handler->InitializeBase();
}

void OobeUI::CurrentScreenChanged(OobeScreenId new_screen) {
  previous_screen_ = current_screen_;

  current_screen_ = new_screen;
  for (Observer& observer : observer_list_)
    observer.OnCurrentScreenChanged(previous_screen_, new_screen);
}

bool OobeUI::IsScreenInitialized(OobeScreenId screen) {
  for (BaseScreenHandler* handler : screen_handlers_) {
    if (handler->oobe_screen() == screen) {
      return handler->page_is_ready();
    }
  }
  return false;
}

bool OobeUI::IsJSReady(base::OnceClosure display_is_ready_callback) {
  if (!ready_)
    ready_callbacks_.AddUnsafe(std::move(display_is_ready_callback));
  return ready_;
}

void OobeUI::ShowOobeUI(bool show) {
  core_handler_->ShowOobeUI(show);

  if (show && oobe_display_chooser_)
    oobe_display_chooser_->TryToPlaceUiOnTouchDisplay();
}

void OobeUI::ShowSigninScreen(SigninScreenHandlerDelegate* delegate) {
  signin_screen_handler_->SetDelegate(delegate);

  signin_screen_handler_->Show(core_handler_->show_oobe_ui());
}

void OobeUI::ForwardAccelerator(std::string accelerator_name) {
  core_handler_->ForwardAccelerator(accelerator_name);
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
  if (oobe_display_chooser_)
    oobe_display_chooser_->TryToPlaceUiOnTouchDisplay();
}

void OobeUI::SetLoginUserCount(int user_count) {
  core_handler_->SetLoginUserCount(user_count);
}

void OobeUI::OnSystemTrayBubbleShown() {
  if (current_screen_ == WelcomeView::kScreenId)
    GetHandler<WelcomeScreenHandler>()->CancelChromeVoxHintIdleDetection();
}

WEB_UI_CONTROLLER_TYPE_IMPL(OobeUI)

}  // namespace chromeos
