// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_display_host_webui.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/ui/focus_ring_controller.h"
#include "ash/booting/booting_animation_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/utility/wm_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/ash/first_run/first_run.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/login/input_events_blocker.h"
#include "chrome/browser/ui/ash/login/login_display_host_mojo.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/install_attributes_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/ash/components/audio/public/cpp/sounds/sounds_manager.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/language_preferences/language_preferences.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "components/account_id/account_id.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/window.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Maximum delay for startup sound after 'loginPromptVisible' signal.
const int kStartupSoundMaxDelayMs = 4000;

// URL which corresponds to the OOBE WebUI.
const char kOobeURL[] = "chrome://oobe/oobe";

// URL which corresponds to the app launch splash WebUI.
const char kAppLaunchSplashURL[] = "chrome://oobe/app-launch-splash";

// Duration of sign-in transition animation.
const int kLoginFadeoutTransitionDurationMs = 700;

// Number of times we try to reload OOBE/login WebUI if it crashes.
const int kCrashCountLimit = 5;

// The default fade out animation time in ms.
const int kDefaultFadeTimeMs = 200;

const char kValidInstallAttributesHistogram[] =
    "Enterprise.InstallAttributes.ValidOnEnrolledDevice";

// A class to observe an implicit animation and invokes the callback after the
// animation is completed.
class AnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  AnimationObserver(const AnimationObserver&) = delete;
  AnimationObserver& operator=(const AnimationObserver&) = delete;

  ~AnimationObserver() override = default;

 private:
  // ui::ImplicitAnimationObserver implementation:
  void OnImplicitAnimationsCompleted() override {
    std::move(callback_).Run();
    delete this;
  }

  base::OnceClosure callback_;
};

// Returns whether the device settings are managed.
bool HasManagedDeviceSettings() {
  if (!DeviceSettingsService::IsInitialized()) {
    CHECK_IS_TEST();
    return false;
  }
  return DeviceSettingsService::Get()->IsDeviceManaged();
}

// Even if oobe is complete we may still want to show it, for example, if there
// are no users registered then the user may want to enterprise enroll.
bool IsOobeComplete() {
  // Oobe is completed and we have a user or we are enterprise enrolled.
  return StartupUtils::IsOobeCompleted() &&
         ((!user_manager::UserManager::Get()->GetUsers().empty() &&
           !HasManagedDeviceSettings()) ||
          ash::InstallAttributes::Get()->IsEnterpriseManaged());
}

// Returns true if signin (not oobe) should be displayed.
bool ShouldShowSigninScreen(OobeScreenId first_screen) {
  return (first_screen == ash::OOBE_SCREEN_UNKNOWN && IsOobeComplete());
}

void MaybeShowDeviceDisabledScreen() {
  DCHECK(LoginDisplayHost::default_host());
  if (!g_browser_process->platform_part()->device_disabling_manager()) {
    // Device disabled check will be done in the DeviceDisablingManager.
    return;
  }

  if (!system::DeviceDisablingManager::
          IsDeviceDisabledDuringNormalOperation()) {
    return;
  }

  LoginDisplayHost::default_host()->StartWizard(
      DeviceDisabledScreenView::kScreenId);
}

void MaybeShowInstallAttributesCorruptedScreen() {
  if (HasManagedDeviceSettings() &&
      !InstallAttributes::Get()->IsDeviceLocked()) {
    LOG(ERROR) << "Corrupted install attributes, showing the TPM error";
    base::UmaHistogramBoolean(kValidInstallAttributesHistogram, false);
    LoginDisplayHost::default_host()->StartWizard(
        InstallAttributesErrorView::kScreenId);
  } else {
    base::UmaHistogramBoolean(kValidInstallAttributesHistogram, true);
  }
}

void MaybeShutdownLoginDisplayHostWebUI() {
  if (!LoginDisplayHost::default_host()) {
    return;
  }
  if (!LoginDisplayHost::default_host()->GetOobeUI()) {
    return;
  }
  if (LoginDisplayHost::default_host()->GetOobeUI()->display_type() !=
      OobeUI::kOobeDisplay) {
    return;
  }
  LoginDisplayHost::default_host()->FinalizeImmediately();
  if (LoginDisplayHost::default_host()) {
    // Tests may be keeping a fake instance.
    CHECK_IS_TEST();
  }
}

// Safely check if the UserContext is stored in the WizardContext;
bool ShouldPreserveUserContext() {
  if (!features::IsOobeAddUserDuringEnrollmentEnabled() ||
      !LoginDisplayHost::default_host()) {
    return false;
  }
  WizardContext* wizard_context =
      LoginDisplayHost::default_host()->GetWizardContext();
  if (!wizard_context || !wizard_context->timebound_user_context_holder) {
    return false;
  }
  return true;
}

// ShowLoginWizard is split into two parts. This function is sometimes called
// from TriggerShowLoginWizardFinish() directly, and sometimes from
// OnLanguageSwitchedCallback() (if locale was updated).
void ShowLoginWizardFinish(
    OobeScreenId first_screen,
    const StartupCustomizationDocument* startup_manifest) {
  TRACE_EVENT0("chromeos", "ShowLoginWizard::ShowLoginWizardFinish");
  // `ShowLoginWizardFinish` can be called as a result of
  // `OnLanguageSwitchedCallback` and it can happen that the browser started to
  // shut down. Return early if this is the case.
  if (browser_shutdown::IsTryingToQuit() ||
      KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    return;
  }

  std::unique_ptr<TimeboundUserContextHolder> user_context;
  if (ShouldShowSigninScreen(first_screen)) {
    if (ShouldPreserveUserContext()) {
      // Move the user context to the local variable before it's destroyed.
      WizardContext* wizard_context =
          LoginDisplayHost::default_host()->GetWizardContext();
      CHECK(wizard_context);
      user_context = std::move(wizard_context->timebound_user_context_holder);
    }
    // Shutdown WebUI host to replace with the Mojo one.
    MaybeShutdownLoginDisplayHostWebUI();
  }

  // TODO(crbug.com/41353468): Move LoginDisplayHost creation out of
  // LoginDisplayHostWebUI, it is not specific to a particular implementation.

  // Create the LoginDisplayHost. Use the views-based implementation only for
  // the sign-in screen.
  LoginDisplayHost* display_host = nullptr;
  if (LoginDisplayHost::default_host()) {
    // Tests may have already allocated an instance for us to use.
    display_host = LoginDisplayHost::default_host();
  } else if (ShouldShowSigninScreen(first_screen)) {
    display_host = new LoginDisplayHostMojo(DisplayedScreen::SIGN_IN_SCREEN);
  } else if (first_screen == ArcVmDataMigrationScreenView::kScreenId) {
    display_host = new LoginDisplayHostMojo(DisplayedScreen::SIGN_IN_SCREEN);
    DCHECK(session_manager::SessionManager::Get());
    session_manager::SessionManager::Get()->NotifyLoginOrLockScreenVisible();
  } else {
    display_host = new LoginDisplayHostWebUI();
  }

  if (features::IsOobeAddUserDuringEnrollmentEnabled() && user_context) {
    // Restore the user context within the wizard context.
    WizardContext* wizard_context = display_host->GetWizardContext();
    CHECK(wizard_context);
    wizard_context->timebound_user_context_holder = std::move(user_context);
  }

  // Restore system timezone.
  std::string timezone;
  if (system::PerUserTimezoneEnabled()) {
    timezone = g_browser_process->local_state()->GetString(
        ::prefs::kSigninScreenTimezone);
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  if (ShouldShowSigninScreen(first_screen)) {
    display_host->StartSignInScreen();
  } else {
    display_host->StartWizard(first_screen);

    // Set initial timezone if specified by customization.
    const std::string customization_timezone =
        startup_manifest->initial_timezone();
    VLOG(1) << "Initial time zone: " << customization_timezone;
    // Apply locale customizations only once to preserve whatever locale
    // user has changed to during OOBE.
    if (!customization_timezone.empty()) {
      timezone = customization_timezone;
    }
  }
  if (!timezone.empty()) {
    system::SetSystemAndSigninScreenTimezone(timezone);
  }

  // This step requires the session manager to have been initialized and login
  // display host to be created.
  DCHECK(session_manager::SessionManager::Get());
  DCHECK(LoginDisplayHost::default_host());
  // Postpone loading wallpaper if the booting animation might be played.
  if (!features::IsBootAnimationEnabled() ||
      session_manager::SessionManager::Get()->session_state() !=
          session_manager::SessionState::OOBE) {
    WallpaperControllerClientImpl::Get()->SetInitialWallpaper();
  }

  MaybeShowInstallAttributesCorruptedScreen();

  // TODO(crbug.com/1105387): Part of initial screen logic.
  MaybeShowDeviceDisabledScreen();
}

struct ShowLoginWizardSwitchLanguageCallbackData {
  explicit ShowLoginWizardSwitchLanguageCallbackData(
      OobeScreenId first_screen,
      const StartupCustomizationDocument* startup_manifest)
      : first_screen(first_screen), startup_manifest(startup_manifest) {}

  const OobeScreenId first_screen;
  const raw_ptr<const StartupCustomizationDocument> startup_manifest;

  // lock UI while resource bundle is being reloaded.
  InputEventsBlocker events_blocker;
};

// Trigger OnLocaleChanged via LocaleUpdateController.
void NotifyLocaleChange() {
  LocaleUpdateController::Get()->OnLocaleChanged();
}

void OnLanguageSwitchedCallback(
    std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> self,
    const locale_util::LanguageSwitchResult& result) {
  TRACE_EVENT0("login", "OnLanguageSwitchedCallback");
  if (!result.success) {
    LOG(WARNING) << "Locale could not be found for '" << result.requested_locale
                 << "'";
  }

  // Notify the locale change.
  NotifyLocaleChange();
  ShowLoginWizardFinish(self->first_screen, self->startup_manifest);
}

// Triggers ShowLoginWizardFinish directly if no locale switch is required
// (`switch_locale` is empty) or after a locale switch otherwise.
void TriggerShowLoginWizardFinish(
    std::string switch_locale,
    std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> data) {
  if (switch_locale.empty()) {
    ShowLoginWizardFinish(data->first_screen, data->startup_manifest);
  } else {
    locale_util::SwitchLanguageCallback callback(
        base::BindOnce(&OnLanguageSwitchedCallback, std::move(data)));

    // Load locale keyboards here. Hardware layout would be automatically
    // enabled.
    locale_util::SwitchLanguage(
        switch_locale, true, true /* login_layouts_only */, std::move(callback),
        ProfileManager::GetActiveUserProfile());
  }
}

// Returns the login screen locale mandated by device policy, or an empty string
// if no policy-specified locale is set.
std::string GetManagedLoginScreenLocale() {
  auto* cros_settings = CrosSettings::Get();
  const base::Value::List* login_screen_locales = nullptr;
  if (!cros_settings->GetList(kDeviceLoginScreenLocales,
                              &login_screen_locales)) {
    return std::string();
  }

  // Currently, only the first element is used. The setting is a list for future
  // compatibility, if dynamically switching locales on the login screen will be
  // implemented.
  if (login_screen_locales->empty() ||
      !login_screen_locales->front().is_string()) {
    return std::string();
  }

  return login_screen_locales->front().GetString();
}

// Disables virtual keyboard overscroll. Login UI will scroll user pods
// into view on JS side when virtual keyboard is shown.
void DisableKeyboardOverscroll() {
  auto* client = ChromeKeyboardControllerClient::Get();
  keyboard::KeyboardConfig config = client->GetKeyboardConfig();
  config.overscroll_behavior = keyboard::KeyboardOverscrollBehavior::kDisabled;
  client->SetKeyboardConfig(config);
}

void ResetKeyboardOverscrollBehavior() {
  auto* client = ChromeKeyboardControllerClient::Get();
  keyboard::KeyboardConfig config = client->GetKeyboardConfig();
  config.overscroll_behavior = keyboard::KeyboardOverscrollBehavior::kDefault;
  client->SetKeyboardConfig(config);
}

// Returns true if we have default audio device.
bool CanPlayStartupSound() {
  AudioDevice device;
  bool found = CrasAudioHandler::Get()->GetPrimaryActiveOutputDevice(&device);
  return found && device.stable_device_id_version &&
         device.type != AudioDeviceType::kOther;
}

// Returns the preferences service.
PrefService* GetLocalState() {
  if (g_browser_process && g_browser_process->local_state()) {
    return g_browser_process->local_state();
  }
  return nullptr;
}

}  // namespace

// static
const char LoginDisplayHostWebUI::kShowLoginWebUIid[] = "ShowLoginWebUI";

// A class to handle special menu key for keyboard driven OOBE.
class LoginDisplayHostWebUI::KeyboardDrivenOobeKeyHandler
    : public ui::EventHandler {
 public:
  KeyboardDrivenOobeKeyHandler() { Shell::Get()->AddPreTargetHandler(this); }

  KeyboardDrivenOobeKeyHandler(const KeyboardDrivenOobeKeyHandler&) = delete;
  KeyboardDrivenOobeKeyHandler& operator=(const KeyboardDrivenOobeKeyHandler&) =
      delete;

  ~KeyboardDrivenOobeKeyHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

 private:
  // ui::EventHandler
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->key_code() == ui::VKEY_F6) {
      SystemTrayClientImpl::Get()->SetPrimaryTrayVisible(false);
      event->StopPropagation();
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, public

LoginDisplayHostWebUI::LoginDisplayHostWebUI()
    : oobe_startup_sound_played_(StartupUtils::IsOobeCompleted()) {
  SessionManagerClient::Get()->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);

  ui::DeviceDataManager::GetInstance()->AddObserver(this);

  // When we wait for WebUI to be initialized we wait for the error screen to be
  // shown or the login or lock screen to be shown.
  session_observation_.Observe(session_manager::SessionManager::Get());

  audio::SoundsManager* manager = audio::SoundsManager::Get();
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  manager->Initialize(static_cast<int>(Sound::kStartup),
                      bundle.GetRawDataResource(IDR_SOUND_STARTUP_WAV),
                      media::AudioCodec::kPCM);
}

LoginDisplayHostWebUI::~LoginDisplayHostWebUI() {
  VLOG(4) << __func__;

  SessionManagerClient::Get()->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);

  if (waiting_for_configuration_) {
    OobeConfiguration::Get()->RemoveObserver(this);
    waiting_for_configuration_ = false;
  }

  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);

  if (login_view_ && login_window_) {
    login_window_->RemoveRemovalsObserver(this);
  }

  ResetKeyboardOverscrollBehavior();

  views::FocusManager::set_arrow_key_traversal_enabled(false);
  ResetLoginWindowAndView();

  CHECK(!views::WidgetObserver::IsInObserverList());
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, LoginDisplayHost:

ExistingUserController* LoginDisplayHostWebUI::GetExistingUserController() {
  if (!existing_user_controller_) {
    CreateExistingUserController();
  }
  return existing_user_controller_.get();
}

gfx::NativeWindow LoginDisplayHostWebUI::GetNativeWindow() const {
  return login_window_ ? login_window_->GetNativeWindow() : nullptr;
}

views::Widget* LoginDisplayHostWebUI::GetLoginWindowWidget() const {
  return login_window_;
}

WebUILoginView* LoginDisplayHostWebUI::GetWebUILoginView() const {
  return login_view_;
}

void LoginDisplayHostWebUI::OnFinalize() {
  DVLOG(1) << "Finalizing LoginDisplayHost. User session starting";

  switch (finalize_animation_type_) {
    case ANIMATION_NONE:
      ShutdownDisplayHost();
      break;
    case ANIMATION_WORKSPACE:
      ScheduleWorkspaceAnimation();
      ShutdownDisplayHost();
      break;
    case ANIMATION_FADE_OUT:
      // Display host is deleted once animation is completed
      // since sign in screen widget has to stay alive.
      ScheduleFadeOutAnimation(kDefaultFadeTimeMs);
      break;
  }
}

void LoginDisplayHostWebUI::OnOobeConfigurationChanged() {
  waiting_for_configuration_ = false;
  OobeConfiguration::Get()->RemoveObserver(this);
  StartWizard(first_screen_);
}

void LoginDisplayHostWebUI::StartWizard(OobeScreenId first_screen) {
  if (!StartupUtils::IsOobeCompleted()) {
    // If `prefs::kOobeStartTime` is not yet stored, then this is the first
    // time OOBE has started.
    if (GetLocalState() &&
        GetLocalState()->GetTime(prefs::kOobeStartTime).is_null()) {
      GetLocalState()->SetTime(prefs::kOobeStartTime, base::Time::Now());
      GetOobeMetricsHelper()->RecordPreLoginOobeFirstStart();
    }

    CHECK(OobeConfiguration::Get());
    if (waiting_for_configuration_) {
      return;
    }
    if (!OobeConfiguration::Get()->CheckCompleted()) {
      waiting_for_configuration_ = true;
      first_screen_ = first_screen;
      OobeConfiguration::Get()->AddAndFireObserver(this);
      VLOG(1) << "Login WebUI >> wizard waiting for configuration check";
      return;
    }
  }

  DisableKeyboardOverscroll();

  TryToPlayOobeStartupSound();

  first_screen_ = first_screen;

  VLOG(1) << "Login WebUI >> wizard";

  if (!login_window_) {
    oobe_load_timer_ = base::ElapsedTimer();
    LoadURL(GURL(kOobeURL));
  }

  DVLOG(1) << "Starting wizard, first_screen: " << first_screen;

  // Create and show the wizard.
  if (wizard_controller_ && !wizard_controller_->is_initialized()) {
    wizard_controller_->Init(first_screen);
  } else if (wizard_controller_) {
    wizard_controller_->AdvanceToScreen(first_screen);
  } else {
    wizard_controller_ = std::make_unique<WizardController>(GetWizardContext());
    NotifyWizardCreated();
    wizard_controller_->Init(first_screen);
  }

  if (ash::features::IsBootAnimationEnabled()) {
    auto* welcome_screen = GetWizardController()->GetScreen<WelcomeScreen>();
    const bool should_show =
        wizard_controller_->current_screen() == welcome_screen;
    if (should_show) {
      ash::Shell::Get()
          ->booting_animation_controller()
          ->ShowAnimationWithEndCallback(base::BindOnce(
              &LoginDisplayHostWebUI::OnViewsBootingAnimationPlayed,
              weak_factory_.GetWeakPtr()));
    }
    // Show the underlying OOBE WebUI and wallpaper so they are ready once
    // animation has finished playing.
    login_window_->Show();
    WallpaperControllerClientImpl::Get()->SetInitialWallpaper();
  }
}

WizardController* LoginDisplayHostWebUI::GetWizardController() {
  return wizard_controller_.get();
}

void LoginDisplayHostWebUI::OnStartUserAdding() {
  NOTREACHED_IN_MIGRATION();
}

void LoginDisplayHostWebUI::CancelUserAdding() {
  NOTREACHED_IN_MIGRATION();
}

void LoginDisplayHostWebUI::OnStartSignInScreen() {
  DisableKeyboardOverscroll();

  finalize_animation_type_ = ANIMATION_WORKSPACE;

  VLOG(1) << "Login WebUI >> sign in";

  DVLOG(1) << "Starting sign in screen";
  CreateExistingUserController();

  existing_user_controller_->Init(user_manager::UserManager::Get()->GetUsers());

  ShowGaiaDialogCommon(EmptyAccountId());

  OnStartSignInScreenCommon();

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0(
      "ui", "WaitForScreenStateInitialize",
      TRACE_ID_WITH_SCOPE(kShowLoginWebUIid, TRACE_ID_GLOBAL(1)));

  // TODO(crbug.com/40549648): Make sure this is ported to views.
  BootTimesRecorder::Get()->RecordCurrentStats(
      "login-wait-for-signin-state-initialize");
}

void LoginDisplayHostWebUI::OnStartAppLaunch() {
  finalize_animation_type_ = ANIMATION_FADE_OUT;
  if (!login_window_) {
    LoadURL(GURL(kAppLaunchSplashURL));
  }

  login_view_->set_should_emit_login_prompt_visible(false);
  if (!wizard_controller_) {
    wizard_controller_ = std::make_unique<WizardController>(GetWizardContext());
    NotifyWizardCreated();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, public

void LoginDisplayHostWebUI::OnBrowserCreated() {
  VLOG(4) << "OnBrowserCreated";
  // Close lock window now so that the launched browser can receive focus.
  ResetLoginWindowAndView();
}

OobeUI* LoginDisplayHostWebUI::GetOobeUI() const {
  if (!login_view_) {
    return nullptr;
  }
  return login_view_->GetOobeUI();
}

content::WebContents* LoginDisplayHostWebUI::GetOobeWebContents() const {
  if (!login_view_) {
    return nullptr;
  }
  return login_view_->GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, WebContentsObserver:

void LoginDisplayHostWebUI::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // Do not try to restore on shutdown
  if (browser_shutdown::HasShutdownStarted()) {
    return;
  }

  crash_count_++;
  if (crash_count_ > kCrashCountLimit) {
    return;
  }

  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    // Render with login screen crashed. Let's crash browser process to let
    // session manager restart it properly. It is hard to reload the page
    // and get to controlled state that is fully functional.
    // If you see check, search for renderer crash for the same client.
    LOG(FATAL) << "Renderer crash on login window";
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, SessionManagerClient::Observer:

void LoginDisplayHostWebUI::EmitLoginPromptVisibleCalled() {
  OnLoginPromptVisible();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, CrasAudioHandler::AudioObserver:

void LoginDisplayHostWebUI::OnActiveOutputNodeChanged() {
  PlayStartupSoundIfPossible();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, display::DisplayObserver:

void LoginDisplayHostWebUI::OnDisplayAdded(
    const display::Display& new_display) {
  if (GetOobeUI()) {
    GetOobeUI()->OnDisplayConfigurationChanged();
  }
}

void LoginDisplayHostWebUI::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  const display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  if (display.id() != primary_display.id() ||
      !(changed_metrics & DISPLAY_METRIC_BOUNDS)) {
    return;
  }

  if (GetOobeUI()) {
    GetOobeUI()->GetCoreOobe()->UpdateClientAreaSize(primary_display.size());
    if (changed_metrics & DISPLAY_METRIC_PRIMARY) {
      GetOobeUI()->OnDisplayConfigurationChanged();
    }
  }
}

void LoginDisplayHostWebUI::OnShowWebUITimeout() {
  VLOG(1) << "Login WebUI >> Show WebUI because of timeout";
  ShowWebUI();
}

void LoginDisplayHostWebUI::OnViewsBootingAnimationPlayed() {
  booting_animation_finished_playing_ = true;
  if (webui_ready_to_take_over_) {
    // This function is called by the AnimationObserver which can't destroy the
    // animation on its own so we need to post a task to do so.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&LoginDisplayHostWebUI::FinishBootingAnimation,
                       weak_factory_.GetWeakPtr()));
  }
}

void LoginDisplayHostWebUI::FinishBootingAnimation() {
  CHECK(features::IsBootAnimationEnabled());
  ash::Shell::Get()->booting_animation_controller()->Finish();
  GetOobeUI()->GetCoreOobe()->TriggerDown();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, ui::InputDeviceEventObserver
void LoginDisplayHostWebUI::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if ((input_device_types & ui::InputDeviceEventObserver::kTouchscreen) &&
      GetOobeUI()) {
    GetOobeUI()->OnDisplayConfigurationChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, views::WidgetRemovalsObserver:
void LoginDisplayHostWebUI::OnWillRemoveView(views::Widget* widget,
                                             views::View* view) {
  if (view != static_cast<views::View*>(login_view_)) {
    return;
  }
  ResetLoginView();
  widget->RemoveRemovalsObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, views::WidgetObserver:
void LoginDisplayHostWebUI::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(login_window_, widget);
  login_window_->RemoveRemovalsObserver(this);
  login_window_->RemoveObserver(this);

  login_window_ = nullptr;
  ResetLoginView();
}

void LoginDisplayHostWebUI::OnWidgetBoundsChanged(views::Widget* widget,
                                                  const gfx::Rect& new_bounds) {
  for (auto& observer : observers_) {
    observer.WebDialogViewBoundsChanged(new_bounds);
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, OobeUI::Observer

void LoginDisplayHostWebUI::OnCurrentScreenChanged(OobeScreenId current_screen,
                                                   OobeScreenId new_screen) {
  if (current_screen == ash::OOBE_SCREEN_UNKNOWN) {
    // Notify that the OOBE page is ready and the first screen is shown. It
    // might happen that front-end part isn't fully initialized yet (when
    // `OobeLazyLoading` is enabled), so wait for it to happen before notifying.
    GetOobeUI()->IsJSReady(base::BindOnce(
        &session_manager::SessionManager::NotifyLoginOrLockScreenVisible,
        base::Unretained(session_manager::SessionManager::Get())));
  }
}

void LoginDisplayHostWebUI::OnBackdropLoaded() {
  webui_ready_to_take_over_ = true;
  if (booting_animation_finished_playing_) {
    FinishBootingAnimation();
  }
}

void LoginDisplayHostWebUI::OnDestroyingOobeUI() {
  GetOobeUI()->RemoveObserver(this);
}

bool LoginDisplayHostWebUI::IsOobeUIDialogVisible() const {
  return true;
}

bool LoginDisplayHostWebUI::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kToggleSystemInfo) {
    if (!GetOobeUI()) {
      return false;
    }
    GetOobeUI()->GetCoreOobe()->ToggleSystemInfo();
    return true;
  }

  return LoginDisplayHostCommon::HandleAccelerator(action);
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, private

void LoginDisplayHostWebUI::ScheduleWorkspaceAnimation() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLoginAnimations)) {
    Shell::Get()->DoInitialWorkspaceAnimation();
  }
}

void LoginDisplayHostWebUI::ScheduleFadeOutAnimation(int animation_speed_ms) {
  // login window might have been closed by OnBrowserCreated() at this moment.
  // This may happen when adding another user into the session, and a browser
  // is created before session start, which triggers the close of the login
  // window. In this case, we should shut down the display host directly.
  if (!login_window_) {
    ShutdownDisplayHost();
    return;
  }
  ui::Layer* layer = login_window_->GetLayer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.AddObserver(new AnimationObserver(
      base::BindOnce(&LoginDisplayHostWebUI::ShutdownDisplayHost,
                     weak_factory_.GetWeakPtr())));
  animation.SetTransitionDuration(base::Milliseconds(animation_speed_ms));
  layer->SetOpacity(0);
}

void LoginDisplayHostWebUI::LoadURL(const GURL& url) {
  InitLoginWindowAndView();
  // Subscribe to crash events.
  content::WebContentsObserver::Observe(login_view_->GetWebContents());
  login_view_->LoadURL(url);
  if (!ash::features::IsBootAnimationEnabled()) {
    login_window_->Show();
  }
  CHECK(GetOobeUI());
  GetOobeUI()->AddObserver(this);
}

void LoginDisplayHostWebUI::ShowWebUI() {
  session_observation_.Reset();
  show_webui_guard_.AbandonAndStop();

  DCHECK(login_window_);
  DCHECK(login_view_);

  VLOG(1) << "Login WebUI >> Show already initialized UI";
  login_window_->Show();
  login_view_->GetWebContents()->Focus();
  login_view_->OnPostponedShow();

  if (oobe_load_timer_.has_value()) {
    base::UmaHistogramTimes("OOBE.WebUI.LoadTime.FirstRun",
                            oobe_load_timer_->Elapsed());
    oobe_load_timer_.reset();
  }
}

void LoginDisplayHostWebUI::InitLoginWindowAndView() {
  if (login_window_) {
    return;
  }

  if (system::InputDeviceSettings::ForceKeyboardDrivenUINavigation()) {
    views::FocusManager::set_arrow_key_traversal_enabled(true);
    focus_ring_controller_ = std::make_unique<FocusRingController>();
    focus_ring_controller_->SetVisible(true);

    keyboard_driven_oobe_key_handler_ =
        std::make_unique<KeyboardDrivenOobeKeyHandler>();
  }

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = CalculateScreenBounds(gfx::Size());
  params.show_state = ui::mojom::WindowShowState::kFullscreen;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  ash_util::SetupWidgetInitParamsForContainer(
      &params, kShellWindowId_LockScreenContainer);
  login_window_ = new views::Widget;
  login_window_->Init(std::move(params));

  login_view_ = new WebUILoginView(weak_factory_.GetWeakPtr());
  login_view_->Init();

  login_window_->SetVisibilityAnimationDuration(
      base::Milliseconds(kLoginFadeoutTransitionDurationMs));
  login_window_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);

  login_window_->AddObserver(this);
  login_window_->AddRemovalsObserver(this);
  login_window_->SetContentsView(login_view_);
  login_window_->GetNativeView()->SetName("WebUILoginView");

  // Delay showing the window until the login webui is ready.
  VLOG(1) << "Login WebUI >> login window is hidden on create";
  login_view_->set_is_hidden(true);

  // A minute should be enough time for the UI to load.
  show_webui_guard_.Start(FROM_HERE, base::Minutes(1), this,
                          &LoginDisplayHostWebUI::OnShowWebUITimeout);
}

void LoginDisplayHostWebUI::ResetLoginWindowAndView() {
  LOG(WARNING) << "ResetLoginWindowAndView";
  // Notify any oobe dialog state observers (e.g. login shelf) that the UI is
  // hidden (so they can reset any cached OOBE dialog state.)
  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::HIDDEN);

  // Make sure to reset the `login_view_` pointer first; it is owned by
  // `login_window_`. Closing `login_window_` could immediately invalidate the
  // `login_view_` pointer.
  if (login_view_) {
    login_view_->SetKeyboardEventsAndSystemTrayEnabled(true);
    ResetLoginView();
  }

  if (login_window_) {
    login_window_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    login_window_->RemoveRemovalsObserver(this);
    login_window_->RemoveObserver(this);
    login_window_ = nullptr;
  }

  // Release wizard controller with the webui and hosting window so that it
  // does not find missing webui handlers in surprise.
  wizard_controller_.reset();
}

void LoginDisplayHostWebUI::TryToPlayOobeStartupSound() {
  need_to_play_startup_sound_ = true;
  PlayStartupSoundIfPossible();
}

void LoginDisplayHostWebUI::ResetLoginView() {
  if (!login_view_) {
    return;
  }

  OobeUI* oobe_ui = login_view_->GetOobeUI();
  if (oobe_ui) {
    oobe_ui->RemoveObserver(this);
  }

  login_view_ = nullptr;
}

void LoginDisplayHostWebUI::OnLoginPromptVisible() {
  if (!login_prompt_visible_time_.is_null()) {
    return;
  }
  login_prompt_visible_time_ = base::TimeTicks::Now();
  TryToPlayOobeStartupSound();
}

void LoginDisplayHostWebUI::CreateExistingUserController() {
  existing_user_controller_ = std::make_unique<ExistingUserController>();
}

void LoginDisplayHostWebUI::ShowGaiaDialog(const AccountId& prefilled_account) {
  ShowGaiaDialogCommon(prefilled_account);
  UpdateWallpaper(prefilled_account);
}

void LoginDisplayHostWebUI::ShowOsInstallScreen() {
  StartWizard(OsInstallScreenView::kScreenId);
}

void LoginDisplayHostWebUI::ShowGuestTosScreen() {
  StartWizard(GuestTosScreenView::kScreenId);
}

void LoginDisplayHostWebUI::ShowRemoteActivityNotificationScreen() {
  StartWizard(RemoteActivityNotificationView::kScreenId);
}

void LoginDisplayHostWebUI::HideOobeDialog(bool saml_page_closed) {
  DUMP_WILL_BE_NOTREACHED();
}

void LoginDisplayHostWebUI::SetShelfButtonsEnabled(bool enabled) {
  LoginScreen::Get()->EnableShelfButtons(enabled);
  if (GetWebUILoginView()) {
    GetWebUILoginView()->set_shelf_enabled(enabled);
  }
}

void LoginDisplayHostWebUI::UpdateOobeDialogState(OobeDialogState state) {
  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(state);
}

void LoginDisplayHostWebUI::HandleDisplayCaptivePortal() {
  GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
}

void LoginDisplayHostWebUI::OnCancelPasswordChangedFlow() {}

void LoginDisplayHostWebUI::UpdateAddUserButtonStatus() {
  NOTREACHED_IN_MIGRATION();
}

void LoginDisplayHostWebUI::RequestSystemInfoUpdate() {
  NOTREACHED_IN_MIGRATION();
}

bool LoginDisplayHostWebUI::HasUserPods() {
  return false;
}

void LoginDisplayHostWebUI::StartUserRecovery(const AccountId& account_id) {
  NOTREACHED_IN_MIGRATION();
}

void LoginDisplayHostWebUI::UseAlternativeAuthentication(
    std::unique_ptr<UserContext> user_context,
    bool online_password_mismatch) {
  DUMP_WILL_BE_NOTREACHED();
}

void LoginDisplayHostWebUI::RunLocalAuthentication(
    std::unique_ptr<UserContext> user_context) {
  NOTREACHED_IN_MIGRATION();
}

void LoginDisplayHostWebUI::AddObserver(LoginDisplayHost::Observer* observer) {
  observers_.AddObserver(observer);
}

void LoginDisplayHostWebUI::RemoveObserver(
    LoginDisplayHost::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LoginDisplayHostWebUI::OnLoginOrLockScreenVisible() {
  VLOG(1) << "Login WebUI >> WEBUI_VISIBLE";
  ShowWebUI();
}

SigninUI* LoginDisplayHostWebUI::GetSigninUI() {
  if (!GetWizardController()) {
    return nullptr;
  }
  return this;
}

bool LoginDisplayHostWebUI::IsWizardControllerCreated() const {
  return wizard_controller_.get();
}

bool LoginDisplayHostWebUI::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) const {
  return false;
}

bool LoginDisplayHostWebUI::IsWebUIStarted() const {
  return true;
}

void LoginDisplayHostWebUI::PlayStartupSoundIfPossible() {
  if (!need_to_play_startup_sound_ || oobe_startup_sound_played_) {
    return;
  }

  if (login_prompt_visible_time_.is_null()) {
    return;
  }

  if (!CanPlayStartupSound()) {
    return;
  }

  need_to_play_startup_sound_ = false;
  oobe_startup_sound_played_ = true;

  const base::TimeDelta time_since_login_prompt_visible =
      base::TimeTicks::Now() - login_prompt_visible_time_;
  base::UmaHistogramTimes("Accessibility.OOBEStartupSoundDelay",
                          time_since_login_prompt_visible);

  // Don't try to play startup sound if login prompt has been already visible
  // for a long time.
  if (time_since_login_prompt_visible >
      base::Milliseconds(kStartupSoundMaxDelayMs)) {
    return;
  }
  AccessibilityManager::Get()->PlayEarcon(Sound::kStartup,
                                          PlaySoundOption::kAlways);
}

////////////////////////////////////////////////////////////////////////////////
// external

// Declared in login_wizard.h so that others don't need to depend on our .h.
// TODO(nkostylev): Split this into a smaller functions.
void ShowLoginWizard(OobeScreenId first_screen) {
  if (browser_shutdown::IsTryingToQuit()) {
    return;
  }

  VLOG(1) << "Showing OOBE screen: " << first_screen;

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();

  if (g_browser_process && g_browser_process->local_state()) {
    manager->GetActiveIMEState()->SetInputMethodLoginDefault();
  }

  system::InputDeviceSettings::Get()->SetNaturalScroll(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault));

  auto session_state = session_manager::SessionState::OOBE;
  if (IsOobeComplete()) {
    session_state = session_manager::SessionState::LOGIN_PRIMARY;
  }
  session_manager::SessionManager::Get()->SetSessionState(session_state);

  // Kiosk launch is handled inside `ChromeSessionManager` code.
  CHECK(first_screen != AppLaunchSplashScreenView::kScreenId);

  // Check whether we need to execute OOBE flow.
  // TODO(b/338302062): Determine whether we should wait on OOBE config
  // retrieval before calling GetPrescribedEnrollmentConfig here.
  const policy::EnrollmentConfig enrollment_config =
      policy::EnrollmentConfig::GetPrescribedEnrollmentConfig();
  if (enrollment_config.should_enroll() &&
      first_screen == ash::OOBE_SCREEN_UNKNOWN) {
    // Manages its own lifetime. See ShutdownDisplayHost().
    auto* display_host = new LoginDisplayHostWebUI();
    // Shows networks screen instead of enrollment screen to resume the
    // interrupted auto start enrollment flow because enrollment screen does
    // not handle flaky network. See http://crbug.com/332572
    display_host->StartWizard(WelcomeView::kScreenId);
    // Make sure we load an initial wallpaper here. If the boot animation
    // might be played it will be covered by the StartWizard call.
    if (!ash::features::IsBootAnimationEnabled()) {
      WallpaperControllerClientImpl::Get()->SetInitialWallpaper();
    }
    return;
  }

  if (StartupUtils::IsEulaAccepted()) {
    DelayNetworkCall(ServicesCustomizationDocument::GetInstance()
                         ->EnsureCustomizationAppliedClosure());

    g_browser_process->platform_part()
        ->GetTimezoneResolverManager()
        ->UpdateTimezoneResolver();
  }

  PrefService* prefs = g_browser_process->local_state();
  std::string current_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&current_locale);
  VLOG(1) << "Current locale: " << current_locale;

  if (ShouldShowSigninScreen(first_screen)) {
    std::string switch_locale = GetManagedLoginScreenLocale();
    if (switch_locale == current_locale) {
      switch_locale.clear();
    }

    std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> data =
        std::make_unique<ShowLoginWizardSwitchLanguageCallbackData>(
            first_screen, nullptr);
    TriggerShowLoginWizardFinish(switch_locale, std::move(data));
    return;
  }

  // Load startup manifest.
  const StartupCustomizationDocument* startup_manifest =
      StartupCustomizationDocument::GetInstance();

  // Switch to initial locale if specified by customization
  // and has not been set yet. We cannot call
  // LanguageSwitchMenu::SwitchLanguage here before
  // EmitLoginPromptReady.
  const std::string& locale = startup_manifest->initial_locale_default();

  const std::string& layout = startup_manifest->keyboard_layout();
  VLOG(1) << "Initial locale: " << locale << " keyboard layout: " << layout;

  // Determine keyboard layout from OEM customization (if provided) or
  // initial locale and save it in preferences.
  manager->GetActiveIMEState()->SetInputMethodLoginDefaultFromVPD(locale,
                                                                  layout);

  std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> data(
      new ShowLoginWizardSwitchLanguageCallbackData(first_screen,
                                                    startup_manifest));

  if (!current_locale.empty() || locale.empty()) {
    TriggerShowLoginWizardFinish(std::string(), std::move(data));
    return;
  }

  // Save initial locale from VPD/customization manifest as current
  // Chrome locale. Otherwise it will be lost if Chrome restarts.
  // Don't need to schedule pref save because setting initial local
  // will enforce preference saving.
  prefs->SetString(language::prefs::kApplicationLocale, locale);
  StartupUtils::SetInitialLocale(locale);

  TriggerShowLoginWizardFinish(locale, std::move(data));
}

void SwitchWebUItoMojo() {
  DCHECK_EQ(LoginDisplayHost::default_host()->GetOobeUI()->display_type(),
            OobeUI::kOobeDisplay);

  // This replaces WebUI host with the Mojo (views) host.
  ShowLoginWizard(ash::OOBE_SCREEN_UNKNOWN);
}

}  // namespace ash
