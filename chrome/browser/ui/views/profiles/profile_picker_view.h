// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

struct AccountInfo;
class Browser;

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
}  // namespace content

// Dialog widget that contains the Desktop Profile picker webui.
class ProfilePickerView : public views::DialogDelegateView,
                          public content::WebContentsDelegate,
                          public signin::IdentityManager::Observer {
 public:
  using BrowserOpenedCallback = base::OnceCallback<void(Browser*)>;

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
                      base::OnceClosure switch_failure_callback);
  // On creation success for the sign-in profile, it rebuilds the view.
  void OnProfileForSigninCreated(SkColor profile_color,
                                 Profile* new_profile,
                                 Profile::CreateStatus status);
  // Switches the layout to the sync confirmation screen.
  void SwitchToSyncConfirmation();

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void WindowClosing() override;

  // views::View;
  gfx::Size GetMinimumSize() const override;

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

  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  // Helper functions to deal with the lack of extended account info.
  void SetExtendedAccountInfoTimeoutForTesting(base::TimeDelta timeout);
  void OnExtendedAccountInfoTimeout(const std::string& email);
  void OnProfileNameAvailable();

  // Finishes the creation flow by marking `profile_being_created_` as fully
  // created, opening a browser window for this profile and calling `callback`.
  void FinishSignedInCreationFlow(BrowserOpenedCallback callback);
  void FinishSignedInCreationFlowImpl(BrowserOpenedCallback callback);

  // Internal callback to finish the last steps of the signed-in creation flow.
  void OnBrowserOpened(BrowserOpenedCallback finish_flow_callback,
                       Profile* profile,
                       Profile::CreateStatus profile_create_status);

  ScopedKeepAlive keep_alive_;
  State state_ = State::kNotStarted;

  // The current WebView object, owned by the view hierarchy.
  views::WebView* web_view_ = nullptr;

  // Assigned a value at the beginning of a signed-in profile creation flow,
  // until the end of the flow (i.e. for the rest of the lifetime of this view).
  Profile* signed_in_profile_being_created_ = nullptr;

  base::string16 name_for_signed_in_profile_;
  base::OnceClosure on_profile_name_available_;
  base::TimeDelta extended_account_info_timeout_;

  // Not null iff switching to sign-in is in progress.
  base::OnceClosure switch_failure_callback_;
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_{this};

  // Creation time of the picker, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;

  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilePickerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
