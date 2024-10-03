// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_WEBUI_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_WEBUI_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host_common.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace ash {
class FocusRingController;
class WebUILoginView;

// An implementation class for OOBE and user adding screen host via WebUI.
// For OOBE, it provides wizard screens such as welcome, network, EULA, update,
// GAIA etc. For user adding, it is legacy support and provides the user
// selection screen (aka account picker).
// The WebUI (chrome://oobe) is loaded hidden on start and made visible when
// WebUI signals ready (via NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE) or there
// is a network error (via NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN).
class LoginDisplayHostWebUI : public LoginDisplayHostCommon,
                              public session_manager::SessionManagerObserver,
                              public content::WebContentsObserver,
                              public SessionManagerClient::Observer,
                              public CrasAudioHandler::AudioObserver,
                              public OobeConfiguration::Observer,
                              public display::DisplayObserver,
                              public ui::InputDeviceEventObserver,
                              public views::WidgetRemovalsObserver,
                              public views::WidgetObserver,
                              public OobeUI::Observer {
 public:
  LoginDisplayHostWebUI();

  LoginDisplayHostWebUI(const LoginDisplayHostWebUI&) = delete;
  LoginDisplayHostWebUI& operator=(const LoginDisplayHostWebUI&) = delete;

  ~LoginDisplayHostWebUI() override;

  // LoginDisplayHost:
  ExistingUserController* GetExistingUserController() override;
  gfx::NativeWindow GetNativeWindow() const override;
  views::Widget* GetLoginWindowWidget() const override;
  OobeUI* GetOobeUI() const override;
  content::WebContents* GetOobeWebContents() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void OnFinalize() override;
  void StartWizard(OobeScreenId first_screen) override;
  WizardController* GetWizardController() override;
  void OnStartUserAdding() override;
  void CancelUserAdding() override;
  void OnStartSignInScreen() override;
  void OnStartAppLaunch() override;
  void OnBrowserCreated() override;
  void ShowGaiaDialog(const AccountId& prefilled_account) override;
  void StartUserRecovery(const AccountId& account_to_recover) override;
  void ShowOsInstallScreen() override;
  void ShowGuestTosScreen() override;
  void ShowRemoteActivityNotificationScreen() override;
  void HideOobeDialog(bool saml_page_closed = false) override;
  void SetShelfButtonsEnabled(bool enabled) override;
  void UpdateOobeDialogState(OobeDialogState state) override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;
  void RequestSystemInfoUpdate() override;
  void OnCancelPasswordChangedFlow() override;
  bool HasUserPods() override;
  void UseAlternativeAuthentication(std::unique_ptr<UserContext> user_context,
                                    bool online_password_mismatch) override;
  void RunLocalAuthentication(
      std::unique_ptr<UserContext> user_context) override;
  void AddObserver(LoginDisplayHost::Observer* observer) override;
  void RemoveObserver(LoginDisplayHost::Observer* observer) override;
  SigninUI* GetSigninUI() final;
  bool IsWizardControllerCreated() const final;
  bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                    int* value) const final;
  bool IsWebUIStarted() const final;

  // LoginDisplayHostCommon:
  bool HandleAccelerator(LoginAcceleratorAction action) final;

  // session_manager::SessionManagerObserver:
  void OnLoginOrLockScreenVisible() override;

  // Trace id for ShowLoginWebUI event (since there exists at most one login
  // WebUI at a time).
  static const char kShowLoginWebUIid[];

  views::Widget* login_window_for_test() { return login_window_; }

 protected:
  class KeyboardDrivenOobeKeyHandler;

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // SessionManagerClient::Observer:
  void EmitLoginPromptVisibleCalled() override;

  // OobeConfiguration::Observer:
  void OnOobeConfigurationChanged() override;

  // CrasAudioHandler::AudioObserver:
  void OnActiveOutputNodeChanged() override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ui::InputDeviceEventObserver
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // views::WidgetRemovalsObserver:
  void OnWillRemoveView(views::Widget* widget, views::View* view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnBackdropLoaded() override;
  void OnDestroyingOobeUI() override;

  // LoginDisplayHostCommon:
  bool IsOobeUIDialogVisible() const override;

 private:
  // Type of animations to run after the login screen.
  enum FinalizeAnimationType {
    ANIMATION_NONE,       // No animation.
    ANIMATION_WORKSPACE,  // Use initial workspace animation (drop and
                          // and fade in workspace). Used for user login.
    ANIMATION_FADE_OUT,   // Fade out login screen. Used for app launch.
  };

  // Schedules workspace transition animation.
  void ScheduleWorkspaceAnimation();

  // Schedules fade out animation.
  void ScheduleFadeOutAnimation(int animation_speed_ms);

  // Loads given URL. Creates WebUILoginView if needed.
  void LoadURL(const GURL& url);

  // Shows OOBE/sign in WebUI that was previously initialized in hidden state.
  void ShowWebUI();

  // Initializes `login_window_` and `login_view_` fields if needed.
  void InitLoginWindowAndView();

  // Closes `login_window_` and resets `login_window_` and `login_view_` fields.
  void ResetLoginWindowAndView();

  // Toggles OOBE progress bar visibility, the bar is hidden by default.
  void SetOobeProgressBarVisible(bool visible);

  // Tries to play startup sound. If sound can't be played right now,
  // for instance, because cras server is not initialized, playback
  // will be delayed.
  void TryToPlayOobeStartupSound();

  // Called when login-prompt-visible signal is caught.
  void OnLoginPromptVisible();

  // Creates or recreates `existing_user_controller_`.
  void CreateExistingUserController();

  // Plays startup sound if needed and audio device is ready.
  void PlayStartupSoundIfPossible();

  // Resets login view and unbinds login display from the signin screen handler.
  void ResetLoginView();

  // Show OOBE WebUI if signal from javascript side never came.
  void OnShowWebUITimeout();

  // Callback that is called once booting animation in views has finished
  // running, but the last frame is still shown.
  void OnViewsBootingAnimationPlayed();

  // Finishes booting animation in views and triggers the WebUI part.
  void FinishBootingAnimation();

  // Sign in screen controller.
  std::unique_ptr<ExistingUserController> existing_user_controller_;

  // OOBE and some screens (camera, recovery) controller.
  std::unique_ptr<WizardController> wizard_controller_;

  // Container of the screen we are displaying.
  raw_ptr<views::Widget> login_window_ = nullptr;

  // Container of the view we are displaying.
  raw_ptr<WebUILoginView> login_view_ = nullptr;

  // Stores status area current visibility to be applied once login WebUI
  // is shown.
  bool status_area_saved_visibility_ = false;

  // True if WebUI is initialized in hidden state, the OOBE is not completed
  // and we're waiting for OOBE configuration check to finish.
  bool waiting_for_configuration_ = false;

  // How many times renderer has crashed.
  int crash_count_ = 0;

  // Stored parameters for StartWizard, required to restore in case of crash.
  OobeScreenId first_screen_ = ash::OOBE_SCREEN_UNKNOWN;

  // A focus ring controller to draw focus ring around view for keyboard
  // driven oobe.
  std::unique_ptr<FocusRingController> focus_ring_controller_;

  // Handles special keys for keyboard driven oobe.
  std::unique_ptr<KeyboardDrivenOobeKeyHandler>
      keyboard_driven_oobe_key_handler_;

  FinalizeAnimationType finalize_animation_type_ = ANIMATION_WORKSPACE;

  // Time when login prompt visible signal is received. Used for
  // calculations of delay before startup sound.
  base::TimeTicks login_prompt_visible_time_;

  // True when request to play startup sound was sent to
  // SoundsManager.
  // After OOBE is completed, this is always initialized with true.
  bool oobe_startup_sound_played_ = false;

  // True if we need to play startup sound when audio device becomes available.
  bool need_to_play_startup_sound_ = false;

  // True if WebUI has loaded the minimum UI that can be shown. It is used to
  // synchronize the booting animation between views and WebUI.
  bool webui_ready_to_take_over_ = false;

  // True if booting animation has finished playing.
  bool booting_animation_finished_playing_ = false;

  // Measures OOBE WebUI load time.
  std::optional<base::ElapsedTimer> oobe_load_timer_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  display::ScopedDisplayObserver display_observer_{this};

  base::ObserverList<LoginDisplayHost::Observer> observers_;

  base::OneShotTimer show_webui_guard_;

  base::WeakPtrFactory<LoginDisplayHostWebUI> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_WEBUI_H_
