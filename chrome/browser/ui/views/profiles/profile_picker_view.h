// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include "base/cancelable_callback.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_host.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

struct AccountInfo;
class Browser;

namespace base {
class FilePath;
}

namespace content {
struct ContextMenuParams;
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Dialog widget that contains the Desktop Profile picker webui.
class ProfilePickerView : public views::WidgetDelegateView,
                          public content::WebContentsDelegate,
                          public signin::IdentityManager::Observer,
                          public ChromeWebModalDialogManagerDelegate,
                          public web_modal::WebContentsModalDialogHost {
 public:
  METADATA_HEADER(ProfilePickerView);

  using BrowserOpenedCallback = base::OnceCallback<void(Browser*)>;

  ProfilePickerView(const ProfilePickerView&) = delete;
  ProfilePickerView& operator=(const ProfilePickerView&) = delete;

  const ui::ThemeProvider* GetThemeProviderForProfileBeingCreated() const;

  // Displays sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  void DisplayErrorMessage();

 private:
  friend class ProfilePicker;

  // To display the Profile picker, use ProfilePicker::Show().
  explicit ProfilePickerView(const GURL& on_select_profile_target_url);
  ~ProfilePickerView() override;

  enum State {
    kNotStarted = 0,
    kInitializing = 1,
    kReady = 2,
    kFinalizing = 3
  };

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

  // Struct holding the data related to the sign-in profile creation flow. These
  // variables are grouped together to simplify reasoning about state.
  // TODO(crbug.com/1180654): Turn it into a separate class with code for all
  // the sign-in logic.
  struct SignInFlow {
    explicit SignInFlow(ProfilePickerView* observer,
                        Profile* profile,
                        SkColor profile_color);
    ~SignInFlow();

    // The web contents backed by `profile`. This is used for displaying the
    // sign-in flow.
    std::unique_ptr<content::WebContents> contents;

    Profile* profile = nullptr;

    // Set for the profile at the very end to avoid coloring the simple toolbar
    // for GAIA sign-in (that uses the ThemeProvider of the current profile).
    // TODO(crbug.com/1180654): Make this private and use the current
    // GetSignInColor() as the getter.
    SkColor profile_color;

    // Controls whether `profile` browser window should be shown at the end of
    // the sign-in flow.
    bool is_aborted = false;

    // Email of the signed-in account. It is set after the user finishes the
    // sign-in flow on GAIA and Chrome receives the account info.
    std::string email;

    std::u16string name_for_signed_in_profile;
    base::OnceClosure on_profile_name_available;

    base::CancelableOnceClosure extended_account_info_timeout_closure;

    base::ScopedObservation<signin::IdentityManager,
                            signin::IdentityManager::Observer>
        identity_manager_observation;
  };

  // Displays the profile picker.
  void Display(ProfilePicker::EntryPoint entry_point);
  // Hides the profile picker.
  void Clear();

  // On system profile creation success, it initializes the view.
  void OnSystemProfileCreated(Profile* system_profile,
                              Profile::CreateStatus status);

  // Creates and shows the dialog.
  void Init(Profile* system_profile);

  // Switches the layout to the sign-in flow (and creates a new profile)
  void SwitchToSignIn(SkColor profile_color,
                      base::OnceCallback<void(bool)> switch_finished_callback);
  // Cancel the sign-in flow and returns back to the main picker screen (if the
  // original EntryPoint was to open the picker).
  void CancelSignIn();
  // On creation success for the sign-in profile, it rebuilds the view.
  void OnProfileForSigninCreated(
      SkColor profile_color,
      base::RepeatingCallback<void(bool)> switch_finished_callback,
      Profile* new_profile,
      Profile::CreateStatus status);
  // Switches the layout to the sync confirmation screen.
  void SwitchToSyncConfirmation();
  void SwitchToSyncConfirmationFinished();
  // Switches the layout to the profile switch screen.
  void SwitchToProfileSwitch(const base::FilePath& profile_path);
  // Switches the layout to the enterprise welcome screen.
  void SwitchToEnterpriseProfileWelcome(
      EnterpriseProfileWelcomeUI::ScreenType type,
      base::OnceCallback<void(bool)> proceed_callback);
  void SwitchToEnterpriseProfileWelcomeFinished(
      EnterpriseProfileWelcomeUI::ScreenType type,
      base::OnceCallback<void(bool)> proceed_callback);

  // views::WidgetDelegate:
  void WindowClosing() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::View* GetContentsView() override;
  std::u16string GetAccessibleWindowTitle() const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnThemeChanged() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;

  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  // ChromeWebModalDialogManagerDelegate
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // Builds the views hieararchy.
  void BuildLayout();

  void UpdateToolbarColor();

  // Shows a screen with `url` in `contents` and potentially `show_toolbar`. If
  // `url` is empty, it only shows `contents` with its currently loaded url. If
  // both `navigation_finished_closure` and `url` is non-empty, the closure is
  // called when the navigation commits (if it never commits such as when the
  // navigation is replaced by another navigation, the closure is never called).
  void ShowScreen(
      content::WebContents* contents,
      const GURL& url,
      bool show_toolbar,
      bool enable_navigating_back = true,
      base::OnceClosure navigation_finished_closure = base::OnceClosure());
  void ShowScreenFinished(
      content::WebContents* contents,
      bool show_toolbar,
      bool enable_navigating_back,
      base::OnceClosure navigation_finished_closure = base::OnceClosure());

  void BackButtonPressed(const ui::Event& event);
  void NavigateBack();

  // Checks whether the sign-in flow is in progress.
  bool GetSigningIn() const;

  // Helper functions to deal with the lack of extended account info.
  void SetExtendedAccountInfoTimeoutForTesting(base::TimeDelta timeout);
  void OnExtendedAccountInfoTimeout(const CoreAccountInfo& account);
  void OnProfileNameAvailable();

  // Finishes the creation flow by marking `profile_being_created_` as fully
  // created, opening a browser window for this profile and calling `callback`.
  void FinishSignedInCreationFlow(BrowserOpenedCallback callback,
                                  bool enterprise_sync_consent_needed);
  void FinishSignedInCreationFlowImpl(BrowserOpenedCallback callback,
                                      bool enterprise_sync_consent_needed);

  // Finishes the flow by finalizing the profile and continuing the SAML sign-in
  // in a browser window.
  void FinishSignedInCreationFlowForSAML();
  void OnSignInContentsFreedUp();

  // Internal callback to finish the last steps of the signed-in creation flow.
  void OnBrowserOpened(BrowserOpenedCallback finish_flow_callback,
                       Profile* profile,
                       Profile::CreateStatus profile_create_status);

  // Register basic keyboard accelerators such as closing the window (Alt-F4
  // on Windows).
  void ConfigureAccelerators();

  // Shows a dialog where the user can auth the profile or see the
  // auth error message. If a dialog is already shown, this destroys the current
  // dialog and creates a new one.
  void ShowDialog(content::BrowserContext* browser_context,
                  const GURL& url,
                  const base::FilePath& profile_path);

  // Hides the dialog if it is showing.
  void HideDialog();

  // Getter of the path of profile which is selected in profile picker for force
  // signin.
  base::FilePath GetForceSigninProfilePath() const;

  // Getter of the target page url. If not empty and is valid, it opens on
  // profile selection instead of the new tab page.
  GURL GetOnSelectProfileTargetUrl() const;

  // Getter of the path of profile which is displayed on the profile switch
  // screen.
  base::FilePath GetSwitchProfilePath() const;

  ScopedKeepAlive keep_alive_;
  ProfilePicker::EntryPoint entry_point_ =
      ProfilePicker::EntryPoint::kOnStartup;
  State state_ = State::kNotStarted;

  // A mapping between accelerators and command IDs.
  std::map<ui::Accelerator, int> accelerator_table_;
  bool enable_navigating_back_ = true;

  // Handler for unhandled key events from renderer.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Views, owned by the view hierarchy.
  views::View* toolbar_ = nullptr;
  views::WebView* web_view_ = nullptr;

  // The web contents backed by the system profile. This is used for displaying
  // the WebUI pages.
  std::unique_ptr<content::WebContents> system_profile_contents_;

  // Observer used for implementing screen switching. Non-null only shorty
  // after switching a screen. Must be below all WebContents instances so that
  // WebContents outlive this observer.
  std::unique_ptr<NavigationFinishedObserver> show_screen_finished_observer_;

  std::unique_ptr<SignInFlow> sign_in_;

  // Delay used for a timeout, may be overridden by tests.
  base::TimeDelta extended_account_info_timeout_;

  // Creation time of the picker, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;

  // Hosts dialog displayed when a locked profile is selected in ProfilePicker.
  ProfilePickerForceSigninDialogHost dialog_host_;

  // A target page url that opens on profile selection instead of the new tab
  // page.
  GURL on_select_profile_target_url_;

  // Path to a profile that should be displayed on the profile switch screen.
  base::FilePath switch_profile_path_;

  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
