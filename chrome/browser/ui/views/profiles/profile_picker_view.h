// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include "base/cancelable_callback.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/user_manager_profile_dialog_host.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

struct AccountInfo;
class Browser;

namespace base {
class FilePath;
}

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Dialog widget that contains the Desktop Profile picker webui.
class ProfilePickerView : public views::WidgetDelegateView,
                          public content::WebContentsDelegate,
                          public signin::IdentityManager::Observer {
 public:
  using BrowserOpenedCallback = base::OnceCallback<void(Browser*)>;

  const ui::ThemeProvider* GetThemeProviderForProfileBeingCreated() const;

 private:
  friend class ProfilePicker;

  // To display the Profile picker, use ProfilePicker::Show().
  ProfilePickerView();
  ~ProfilePickerView() override;

  enum State {
    kNotStarted = 0,
    kInitializing = 1,
    kReady = 2,
    kFinalizing = 3
  };

  // Displays the profile picker.
  void Display(ProfilePicker::EntryPoint entry_point);
  // Hides the profile picker.
  void Clear();

  // On system profile creation success, it initializes the view.
  void OnSystemProfileCreated(ProfilePicker::EntryPoint entry_point,
                              Profile* system_profile,
                              Profile::CreateStatus status);

  // Creates and shows the dialog.
  void Init(ProfilePicker::EntryPoint entry_point, Profile* system_profile);

  // Switches the layout to the sign-in flow (and creates a new profile)
  void SwitchToSignIn(SkColor profile_color,
                      base::OnceCallback<void(bool)> switch_finished_callback);
  // On creation success for the sign-in profile, it rebuilds the view.
  void OnProfileForSigninCreated(Profile* new_profile,
                                 Profile::CreateStatus status);
  // Switches the layout to the sync confirmation screen.
  void SwitchToSyncConfirmation();

  // views::WidgetDelegate:
  void WindowClosing() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::View* GetContentsView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

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

  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  // Builds the views hieararchy.
  void BuildLayout();

  // Shows a screen with `url` in `contents` and potentially `show_toolbar`. If
  // `url` is empty, it only shows `contents` with its currently loaded url.
  void ShowScreen(content::WebContents* contents,
                  const GURL& url,
                  bool show_toolbar);

  void BackButtonPressed(const ui::Event& event);

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

  // Displays sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  void DisplayErrorMessage();

  // Getter of the path of profile which is selected in profile picker for force
  // signin.
  base::FilePath GetForceSigninProfilePath();

  ScopedKeepAlive keep_alive_;
  State state_ = State::kNotStarted;

  // A mapping between accelerators and command IDs.
  std::map<ui::Accelerator, int> accelerator_table_;

  // Handler for unhandled key events from renderer.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Views, owned by the view hierarchy.
  views::View* toolbar_ = nullptr;
  views::WebView* web_view_ = nullptr;

  // The web contents backed by the system profile. This is used for displaying
  // the WebUI pages.
  std::unique_ptr<content::WebContents> system_profile_contents_;

  // Assigned a value at the beginning of a signed-in profile creation flow,
  // until the end of the flow (i.e. for the rest of the lifetime of this view).
  Profile* signed_in_profile_being_created_ = nullptr;
  // The web contents backed by `signed_in_profile_being_created_`, with the
  // same lifetime. This is used for displaying the sign-in flow.
  std::unique_ptr<content::WebContents> new_profile_contents_;

  // Assigned a value at the beginning of a signed-in profile creation, set for
  // the profile at the very end to avoid coloring the simple toolbar for GAIA
  // sign-in (that uses the ThemeProvider of the current profile).
  SkColor profile_color_;

  base::string16 name_for_signed_in_profile_;
  base::OnceClosure on_profile_name_available_;
  base::TimeDelta extended_account_info_timeout_;
  base::CancelableOnceClosure extended_account_info_timeout_closure_;

  // Not null iff switching to sign-in is in progress.
  base::OnceCallback<void(bool)> switch_finished_callback_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Creation time of the picker, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;

  // Hosts dialog displayed when a locked profile is selected in ProfilePicker.
  UserManagerProfileDialogHost dialog_host_;

  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilePickerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
