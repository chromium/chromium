// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_host.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
                          public web_modal::WebContentsModalDialogHost {
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void SetNativeToolbarVisible(bool visible) override;
  SkColor GetPreferredBackgroundColor() const override;
#endif

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
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
  gfx::Size GetMinimumSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Gets called when the native wiget changes size.
  // TODO(crbug.com/1380808): Remove once the cause of the bug is found.
  virtual void OnNativeWidgetSizeChanged(const gfx::Size& new_size) {}

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

  // TODO(crbug.com/1380808): Make private once the cause of the bug is found.
  gfx::Size CalculatePreferredSize() const override;

 private:
  friend class ProfilePicker;
  FRIEND_TEST_ALL_PREFIXES(ProfilePickerCreationFlowBrowserTest,
                           CreateForceSignedInProfile);

  class NavigationFinishedObserver : public content::WebContentsObserver {
   public:
    NavigationFinishedObserver(const GURL& url,
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
    const GURL url_;
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Switches the layout to the sign-in screen (and creates a new profile).
  // absl::nullopt `profile_color` corresponds to the default theme.
  void SwitchToDiceSignIn(
      absl::optional<SkColor> profile_color,
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
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SwitchToSignedInFlow(Profile* signed_in_profile,
                            absl::optional<SkColor> profile_color,
                            std::unique_ptr<content::WebContents> contents);
#endif

  // Builds the views hieararchy.
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
  // `ProfilePickerView::Clear()`) the view. It is the owner's responsibility to
  // make sure that the `ProfilePickerView` is still alive and that the callback
  // is valid, before running it.
  ClearHostClosure GetClearClosure();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when the user selects an account on the Lacros-specific account
  // selection screen. Only called for existing profiles, not as part of profile
  // creation.
  void NotifyAccountSelected(const std::string& gaia_id);
#endif

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

  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
