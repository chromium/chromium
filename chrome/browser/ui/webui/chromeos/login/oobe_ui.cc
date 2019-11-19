// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/active_directory_password_change_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_kiosk_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/multidevice_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_board_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/user_image_source.h"
#include "chrome/browser/ui/webui/chromeos/video_source.h"
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
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/display/display.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"

namespace chromeos {

namespace {

const char* kKnownDisplayTypes[] = {OobeUI::kAppLaunchSplashDisplay,
                                    OobeUI::kArcKioskSplashDisplay,
                                    OobeUI::kDiscoverDisplay,
                                    OobeUI::kGaiaSigninDisplay,
                                    OobeUI::kLoginDisplay,
                                    OobeUI::kOobeDisplay,
                                    OobeUI::kUserAddingDisplay};

// Sorted
constexpr char kArcAssistantLogoPath[] = "assistant_logo.png";
constexpr char kArcOverlayCSSPath[] = "overlay.css";
constexpr char kArcPlaystoreCSSPath[] = "playstore.css";
constexpr char kArcPlaystoreJSPath[] = "playstore.js";
constexpr char kArcPlaystoreLogoPath[] = "playstore.svg";
constexpr char kArcSupervisionIconPath[] = "supervision_icon.png";
constexpr char kCustomElementsHTMLPath[] = "custom_elements.html";
constexpr char kCustomElementsJSPath[] = "custom_elements.js";
constexpr char kDiscoverJSPath[] = "discover_app.js";
constexpr char kKeyboardUtilsJSPath[] = "keyboard_utils.js";
constexpr char kLoginJSPath[] = "login.js";
constexpr char kOobeJSPath[] = "oobe.js";
constexpr char kProductLogoPath[] = "product-logo.png";
constexpr char kRecommendAppListViewHTMLPath[] = "recommend_app_list_view.html";
constexpr char kRecommendAppListViewJSPath[] = "recommend_app_list_view.js";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kLogo24PX1XSvgPath[] = "logo_24px-1x.svg";
constexpr char kLogo24PX2XSvgPath[] = "logo_24px-2x.svg";
constexpr char kSyncConsentIcons[] = "sync-consent-icons.html";
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
  source->AddResourcePath(kRecommendAppListViewHTMLPath,
                          IDR_ARC_SUPPORT_RECOMMEND_APP_LIST_VIEW_HTML);
}

void AddAssistantScreensResources(content::WebUIDataSource* source) {
  source->AddResourcePath("voice_match_animation.json",
                          IDR_ASSISTANT_VOICE_MATCH_ANIMATION);
  source->AddResourcePath("voice_match_already_setup_animation.json",
                          IDR_ASSISTANT_VOICE_MATCH_ALREADY_SETUP_ANIMATION);
  source->OverrideContentSecurityPolicyWorkerSrc("worker-src blob: 'self';");
}

void AddFingerprintResources(content::WebUIDataSource* source) {
  int animation_id;
  bool is_lottie_animation = false;
  switch (quick_unlock::GetFingerprintLocation()) {
    case quick_unlock::FingerprintLocation::TABLET_POWER_BUTTON:
      is_lottie_animation = true;
      animation_id = IDR_LOGIN_FINGER_PRINT_TABLET_ANIMATION;
      break;
    case quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
      animation_id =
          IDR_LOGIN_FINGERPRINT_SCANNER_LAPTOP_BOTTOM_RIGHT_ANIMATION;
      break;
    case quick_unlock::FingerprintLocation::KEYBOARD_TOP_RIGHT:
      animation_id = IDR_LOGIN_FINGERPRINT_SCANNER_LAPTOP_TOP_RIGHT_ANIMATION;
      break;
  }
  if (is_lottie_animation) {
    source->AddResourcePath("fingerprint_scanner_animation.json", animation_id);

    // To use lottie, the worker-src CSP needs to be updated for the web ui that
    // is using it. Since as of now there are only a couple of webuis using
    // lottie animations, this update has to be performed manually. As the usage
    // increases, set this as the default so manual override is no longer
    // required.
    source->OverrideContentSecurityPolicyWorkerSrc("worker-src blob: 'self';");
  } else {
    source->AddResourcePath("fingerprint_scanner_animation.png", animation_id);
  }

  source->AddBoolean("useLottieAnimationForFingerprint", is_lottie_animation);
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

// Default and non-shared resource definition for kDiscoverDisplay display type.
// chrome://oobe/discover
void AddDiscoverDisplayTypeDefaultResources(content::WebUIDataSource* source) {
  source->SetDefaultResource(IDR_CHROMEOS_DISCOVER_APP_HTML);
  source->AddResourcePath(kDiscoverJSPath, IDR_CHROMEOS_DISCOVER_APP_JS);
  source->AddResourcePath("manifest.json", IDR_CHROMEOS_DISCOVER_MANIFEST);
  source->AddResourcePath("logo.png", IDR_DISCOVER_APP_192);
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

  // First, configure default and non-shared resources for the current display
  // type.
  if (display_type == OobeUI::kOobeDisplay) {
    AddOobeDisplayTypeDefaultResources(source);
  } else if (display_type == OobeUI::kLockDisplay) {
    NOTREACHED();
  } else if (display_type == OobeUI::kDiscoverDisplay) {
    AddDiscoverDisplayTypeDefaultResources(source);
  } else {
    AddLoginDisplayTypeDefaultResources(source);
  }

  // Configure shared resources
  AddProductLogoResources(source);

  AddFingerprintResources(source);
  AddSyncConsentResources(source);
  AddArcScreensResources(source);
  AddAssistantScreensResources(source);

  source->AddResourcePath(kKeyboardUtilsJSPath, IDR_KEYBOARD_UTILS_JS);
  source->OverrideContentSecurityPolicyObjectSrc(
      "object-src chrome:;");

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

bool IsRemoraRequisitioned() {
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  return policy_manager && policy_manager->IsRemoraRequisition();
}

void DisablePolymer2(content::URLDataSource* shared_source) {
  if (shared_source)
    shared_source->DisablePolymer2ForHost(chrome::kChromeUIOobeHost);
}

}  // namespace

// static
const char OobeUI::kAppLaunchSplashDisplay[] = "app-launch-splash";
const char OobeUI::kArcKioskSplashDisplay[] = "arc-kiosk-splash";
const char OobeUI::kDiscoverDisplay[] = "discover";
const char OobeUI::kGaiaSigninDisplay[] = "gaia-signin";
const char OobeUI::kLockDisplay[] = "lock";
const char OobeUI::kLoginDisplay[] = "login";
const char OobeUI::kOobeDisplay[] = "oobe";
const char OobeUI::kUserAddingDisplay[] = "user-adding";

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

  AddScreenHandler(std::make_unique<HIDDetectionScreenHandler>(
      js_calls_container_.get(), core_handler_));

  AddScreenHandler(
      std::make_unique<ErrorScreenHandler>(js_calls_container_.get()));

  error_screen_ = std::make_unique<ErrorScreen>(GetView<ErrorScreenHandler>());
  ErrorScreen* error_screen = error_screen_.get();

  AddScreenHandler(std::make_unique<EnrollmentScreenHandler>(
      js_calls_container_.get(), network_state_informer_, error_screen));

  AddScreenHandler(std::make_unique<TermsOfServiceScreenHandler>(
      js_calls_container_.get(), core_handler_));

  AddScreenHandler(
      std::make_unique<SyncConsentScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<ArcTermsOfServiceScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<RecommendAppsScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<AppDownloadingScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<UserBoardScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<DemoSetupScreenHandler>(js_calls_container_.get()));

  AddScreenHandler(std::make_unique<DemoPreferencesScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(std::make_unique<FingerprintSetupScreenHandler>(
      js_calls_container_.get()));

  AddScreenHandler(
      std::make_unique<MarketingOptInScreenHandler>(js_calls_container_.get()));

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  ActiveDirectoryPasswordChangeScreenHandler*
      active_directory_password_change_screen_handler = nullptr;
  // Create Active Directory password change screen for corresponding devices
  // only.
  if (connector->IsActiveDirectoryManaged()) {
    auto password_change_handler =
        std::make_unique<ActiveDirectoryPasswordChangeScreenHandler>(
            js_calls_container_.get(), core_handler_);
    active_directory_password_change_screen_handler =
        password_change_handler.get();
    AddScreenHandler(std::move(password_change_handler));
  }

  AddScreenHandler(std::make_unique<GaiaScreenHandler>(
      js_calls_container_.get(), core_handler_, network_state_informer_,
      active_directory_password_change_screen_handler));

  auto signin_screen_handler = std::make_unique<SigninScreenHandler>(
      js_calls_container_.get(), network_state_informer_, error_screen,
      core_handler_, GetHandler<GaiaScreenHandler>());
  signin_screen_handler_ = signin_screen_handler.get();
  AddWebUIHandler(std::move(signin_screen_handler));

  AddScreenHandler(std::make_unique<AppLaunchSplashScreenHandler>(
      js_calls_container_.get(), network_state_informer_, error_screen));

  AddScreenHandler(
      std::make_unique<ArcKioskSplashScreenHandler>(js_calls_container_.get()));

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

  if (IsRemoraRequisitioned())
    oobe_display_chooser_ = std::make_unique<OobeDisplayChooser>();
}

service_manager::Connector* OobeUI::GetLoggedInUserMojoConnector() {
  // This function should only be called after the user has logged in.
  DCHECK(
      user_manager::UserManager::Get()->IsUserLoggedIn() &&
      user_manager::UserManager::Get()->GetActiveUser()->is_profile_created());
  return content::BrowserContext::GetConnectorFor(
      ProfileManager::GetActiveUserProfile());
}

void OobeUI::BindMultiDeviceSetup(
    mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  GetLoggedInUserMojoConnector()->Connect(
      multidevice_setup::mojom::kServiceName, std::move(receiver));
}

void OobeUI::BindPrivilegedHostDeviceSetter(
    mojo::PendingReceiver<multidevice_setup::mojom::PrivilegedHostDeviceSetter>
        receiver) {
  GetLoggedInUserMojoConnector()->Connect(
      multidevice_setup::mojom::kServiceName, std::move(receiver));
}

void OobeUI::BindCrosNetworkConfig(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

OobeUI::OobeUI(content::WebUI* web_ui, const GURL& url)
    : ui::MojoWebUIController(web_ui, true /* enable_chrome_send */) {
  display_type_ = GetDisplayType(url);

  js_calls_container_ = std::make_unique<JSCallsContainer>();

  auto core_handler =
      std::make_unique<CoreOobeHandler>(js_calls_container_.get());
  core_handler_ = core_handler.get();

  AddWebUIHandler(std::move(core_handler));

  if (display_type_ != OobeUI::kDiscoverDisplay)
    ConfigureOobeDisplay();

  AddScreenHandler(
      std::make_unique<DiscoverScreenHandler>(js_calls_container_.get()));

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  // Set up the chrome://oobe/ source.
  content::WebUIDataSource* html_source =
      CreateOobeUIDataSource(localized_strings, display_type_);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  // If allowed, request that the shared resources send this page Polymer 1
  // resources instead of Polymer 2.
  // TODO (https://crbug.com/739611): Remove this exception by migrating to
  // Polymer 2.
  if (base::FeatureList::IsEnabled(features::kWebUIPolymer2Exceptions)) {
    content::URLDataSource::GetSourceForURL(
        Profile::FromWebUI(web_ui),
        GURL("chrome://resources/polymer/v1_0/polymer/polymer.html"),
        base::BindOnce(DisablePolymer2));
  }

  AddHandlerToRegistry(base::BindRepeating(&OobeUI::BindMultiDeviceSetup,
                                           base::Unretained(this)));
  AddHandlerToRegistry(base::BindRepeating(
      &OobeUI::BindPrivilegedHostDeviceSetter, base::Unretained(this)));
  AddHandlerToRegistry(base::BindRepeating(&OobeUI::BindCrosNetworkConfig,
                                           base::Unretained(this)));
}

OobeUI::~OobeUI() {
  for (Observer& observer : observer_list_)
    observer.OnDestroyingOobeUI();
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  localized_strings->SetString("buildType", "chrome");
#else
  localized_strings->SetString("buildType", "chromium");
#endif

  bool keyboard_driven_oobe =
      system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
  localized_strings->SetString("highlightStrength",
                               keyboard_driven_oobe ? "strong" : "normal");

  localized_strings->SetString(
      "showViewsLock", ash::switches::IsUsingViewsLock() ? "on" : "off");
  localized_strings->SetString(
      "showViewsLogin", ash::features::IsViewsLoginEnabled() ? "on" : "off");
  localized_strings->SetBoolean(
      "changePictureVideoModeEnabled",
      base::FeatureList::IsEnabled(features::kChangePictureVideoMode));
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
  for (size_t i = 0; i < ready_callbacks_.size(); ++i)
    ready_callbacks_[i].Run();
  ready_callbacks_.clear();

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

bool OobeUI::IsJSReady(const base::Closure& display_is_ready_callback) {
  if (!ready_)
    ready_callbacks_.push_back(display_is_ready_callback);
  return ready_;
}

void OobeUI::ShowOobeUI(bool show) {
  core_handler_->ShowOobeUI(show);

  if (show && oobe_display_chooser_)
    oobe_display_chooser_->TryToPlaceUiOnTouchDisplay();
}

void OobeUI::ShowSigninScreen(const LoginScreenContext& context,
                              SigninScreenHandlerDelegate* delegate,
                              NativeWindowDelegate* native_window_delegate) {
  // Check our device mode.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->GetDeviceMode() == policy::DEVICE_MODE_LEGACY_RETAIL_MODE) {
    // If we're in legacy retail mode, the best thing we can do is launch the
    // new offline demo mode.
    LoginDisplayHost::default_host()->StartDemoAppLaunch();
    return;
  }

  signin_screen_handler_->SetDelegate(delegate);
  signin_screen_handler_->SetNativeWindowDelegate(native_window_delegate);

  LoginScreenContext actual_context(context);
  signin_screen_handler_->Show(actual_context, core_handler_->show_oobe_ui());
}

void OobeUI::ForwardAccelerator(std::string accelerator_name) {
  core_handler_->ForwardAccelerator(accelerator_name);
}

void OobeUI::ResetSigninScreenHandlerDelegate() {
  signin_screen_handler_->SetDelegate(nullptr);
  signin_screen_handler_->SetNativeWindowDelegate(nullptr);
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

}  // namespace chromeos
