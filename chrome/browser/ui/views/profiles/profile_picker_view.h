// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_host.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ProfilePickerDiceSignInToolbar;
#endif

class Profile;
class ScopedProfileKeepAlive;
class ProfileManagementFlowController;
class ProfilePickerFlowController;
class Browser;
class ProfilePickerFeaturePromoController;
class ForceSigninUIError;

namespace content {
struct ContextMenuParams;
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Dialog widget that contains the Desktop Profile picker webui.
class ProfilePickerView : public views::WidgetDelegateView,
                          public ProfilePickerWebContentsHost,
                          public content::WebContentsDelegate,
                          public web_modal::WebContentsModalDialogHost,
                          public ui::AcceleratorProvider {
  METADATA_HEADER(ProfilePickerView, views::WidgetDelegateView)

 public:
  ProfilePickerView(const ProfilePickerView&) = delete;
  ProfilePickerView& operator=(const ProfilePickerView&) = delete;

  // Updates the parameters. This calls existing callbacks with error values,
  // and requires `ProfilePicker::Params::CanReusePickerWindow()` to be true.
  void UpdateParams(ProfilePicker::Params&& params);

  // Displays sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  void DisplayErrorMessage();

  // ProfilePickerWebContentsHost:
  void ShowScreen(content::WebContents* contents,
                  const GURL& url,
                  base::OnceClosure navigation_finished_closure =
                      base::OnceClosure()) override;
  void ShowScreenInPickerContents(
      const GURL& url,
      base::OnceClosure navigation_finished_closure =
          base::OnceClosure()) override;
  bool ShouldUseDarkColors() const override;
  content::WebContents* GetPickerContents() const override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  content::WebContentsDelegate* GetWebContentsDelegate() override;
  void Reset(StepSwitchFinishedCallback callback) override;
  void ShowForceSigninErrorDialog(const ForceSigninUIError& error,
                                  bool success) override;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void SetNativeToolbarVisible(bool visible) override;
  bool IsNativeToolbarVisibleForTesting() const;
  SkColor GetPreferredBackgroundColor() const override;
#endif

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // views::WidgetDelegateView:
  void WindowClosing() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::View* GetContentsView() override;
  std::u16string GetAccessibleWindowTitle() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // ui::AcceleratorProvider
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // Exposed for testing
  enum State {
    // The view has just been created.
    kNotStarted = 0,

    // The view received a request to be displayed, but nothing has been shown
    // yet.
    kInitializing = 1,

    // The view is currently shown.
    kDisplayed = 2,

    // The view received a request to close, and will be deleted shortly.
    kClosing = 3
  };

  State state_for_testing() { return state_; }
  content::WebContents* get_dialog_web_contents_for_testing() const {
    return dialog_host_.get_web_contents_for_testing();
  }

 protected:
  // To display the Profile picker, use ProfilePicker::Show().
  explicit ProfilePickerView(ProfilePicker::Params&& params);
  ~ProfilePickerView() override;

  // Displays the profile picker.
  void Display();

  // Creates a `ProfileManagementFlowController` to drive the flow for which
  // this profile picker is being shown.
  virtual std::unique_ptr<ProfileManagementFlowController> CreateFlowController(
      Profile* picker_profile,
      ClearHostClosure clear_host_callback);

 private:
  friend class ProfilePicker;
  FRIEND_TEST_ALL_PREFIXES(ProfilePickerCreationFlowBrowserTest,
                           CreateForceSignedInProfile);

  class NavigationFinishedObserver : public content::WebContentsObserver {
   public:
    NavigationFinishedObserver(const GURL& requested_url,
                               base::OnceClosure closure,
                               content::WebContents* contents);
    NavigationFinishedObserver(const NavigationFinishedObserver&) = delete;
    NavigationFinishedObserver& operator=(const NavigationFinishedObserver&) =
        delete;
    ~NavigationFinishedObserver() override;

    // content::WebContentsObserver:
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;

   private:
    const GURL requested_url_;
    base::OnceClosure closure_;
  };

  // If the picker needs to be re-opened, this function schedules the reopening,
  // closes the picker and return true. Otherwise, it returns false.
  bool MaybeReopen(ProfilePicker::Params& params);

  // Closes the profile picker.
  void Clear();

  // On picker profile creation success, it initializes the view.
  void OnPickerProfileCreated(Profile* picker_profile);

  // Creates and shows the dialog.
  void Init(Profile* picker_profile);

  // Finalizes the Init (entering the `kDisplayed` state), called along with the
  // first time `ShowScreen()`.
  void FinishInit();

  // Switch to the flow that comes when the user decides to create a profile
  // without signing in.
  // `profile_color` is the profile's color. It is undefined for the default
  // theme.
  // `profile_picked_time_on_startup` is the time when the user picked a
  // profile to open, to measure browser startup performance. It is only set
  // when the picker is shown on startup.
  void SwitchToSignedOutPostIdentityFlow(
      std::optional<SkColor> profile_color,
      base::TimeTicks profile_picked_time_on_startup,
      base::OnceCallback<void(bool)> switch_finished_callback);

  // Callback used when the profile is created in the signed out flow.
  void OnLocalProfileInitialized(
      std::optional<SkColor> profile_color,
      base::TimeTicks profile_picked_time_on_startup,
      base::OnceCallback<void(bool)> switch_finished_callback,
      Profile* profile);

  // Callback used when the browser is launched after finishing the signed out
  // flow.
  void ShowLocalProfileCustomization(
      base::TimeTicks profile_picked_time_on_startup,
      Browser* browser);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Switches the layout to the sign-in screen (and creates a new profile or
  // load an existing one based on the `profile_info` content).
  void SwitchToDiceSignIn(
      ProfilePicker::ProfileInfo profile_info,
      base::OnceCallback<void(bool)> switch_finished_callback);

  // Starts the forced sign-in flow (and creates a new profile).
  // `switch_finished_callback` gets informed whether the creation of the new
  // profile succeeded and the sign-in UI gets displayed.
  void SwitchToForcedSignIn(
      base::OnceCallback<void(bool)> switch_finished_callback);

  // Handles profile creation when forced sign-in is enabled.
  void OnProfileForDiceForcedSigninCreated(
      base::OnceCallback<void(bool)> switch_finished_callback,
      Profile* new_profile);
  // Switches the profile picker layout to display the reauth page to the main
  // account of the given `profile` if needed. On success the `profile` is
  // unlocked and a browser is opend. On failure the user is redirected to the
  // profile picker main page with an popup error dialog displayed through
  // `on_error_callback`.
  void SwitchToReauth(
      Profile* profile,
      base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SwitchToSignedInFlow(Profile* signed_in_profile,
                            std::optional<SkColor> profile_color,
                            std::unique_ptr<content::WebContents> contents);
#endif

  // Builds the views hierarchy.
  void BuildLayout();

  void ShowScreenFinished(
      content::WebContents* contents,
      base::OnceClosure navigation_finished_closure = base::OnceClosure());

  void NavigateBack();

  // Register basic keyboard accelerators such as closing the window (Alt-F4
  // on Windows).
  void ConfigureAccelerators();

  // Shows a dialog where the user can auth the profile or see the
  // auth error message. If a dialog is already shown, this destroys the current
  // dialog and creates a new one.
  void ShowDialog(Profile* profile, const GURL& url);

  // Hides the dialog if it is showing.
  void HideDialog();

  // Getter of the target page url. If not empty and is valid, it opens on
  // profile selection instead of the new tab page.
  GURL GetOnSelectProfileTargetUrl() const;

  ProfilePickerFlowController* GetProfilePickerFlowController() const;

  // Returns a closure that can be executed to clear (see
  // `ProfilePickerView::Clear()`) the view. Uses a weak pointer internally, so
  // it can be called after the view has been destroyed. It is different from
  // `ProfilePicker::Hide()` because it only clears this specific instance of
  // the picker view, whereas `Hide()` would close any picker view.
  ClearHostClosure GetClearClosure();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when the user selects an account on the Lacros-specific account
  // selection screen. Only called for existing profiles, not as part of profile
  // creation.
  void NotifyAccountSelected(const std::string& gaia_id);
#endif

  // Create the feature promo that manages the IPH logic that can be displayed
  // through the Profile Picker.
  void InitializeFeaturePromo(Profile* system_profile);

  ScopedKeepAlive keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  State state_ = State::kNotStarted;

  // During destruction, `params_` should stay alive longer than
  // `signed_in_flow_` (below) as the flow may want to trigger a callback owned
  // by `params_`.
  ProfilePicker::Params params_;

  // Callback that gets called (if set) when the current window has closed -
  // used to reshow the picker (with different params).
  base::OnceClosure restart_on_window_closing_;

  // A mapping between accelerators and command IDs.
  std::map<ui::Accelerator, int> accelerator_table_;

  // Handler for unhandled key events from renderer.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Owned by the view hierarchy.
  raw_ptr<views::WebView> web_view_ = nullptr;

  // The web contents backed by the picker profile (mostly the system profile).
  // This is used for displaying the WebUI pages.
  std::unique_ptr<content::WebContents> contents_;

  // Observer used for implementing screen switching. Non-null only shorty
  // after switching a screen. Must be below all WebContents instances so that
  // WebContents outlive this observer.
  std::unique_ptr<NavigationFinishedObserver> show_screen_finished_observer_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Toolbar view displayed on top of the WebView for GAIA sign-in, owned by the
  // view hierarchy.
  raw_ptr<ProfilePickerDiceSignInToolbar> toolbar_ = nullptr;
#endif

  std::unique_ptr<ProfileManagementFlowController> flow_controller_;

  // Creation time of the picker, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;

  // Hosts dialog displayed when a locked profile is selected in ProfilePicker.
  ProfilePickerForceSigninDialogHost dialog_host_;

  // Manages IPH promos displayed through the Profile Picker.
  std::unique_ptr<ProfilePickerFeaturePromoController> feature_promo_;

  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
