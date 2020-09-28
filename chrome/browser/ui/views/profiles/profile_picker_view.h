// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
}  // namespace content

// Dialog widget that contains the Desktop Profile picker webui.
class ProfilePickerView : public views::DialogDelegateView,
                          public content::WebContentsDelegate {
 private:
  friend class ProfilePicker;

  // To display the Profile picker, use ProfilePicker::Show().
  ProfilePickerView();
  ~ProfilePickerView() override;

  enum InitState {
    kNotInitialized = 0,
    kInProgress = 1,
    kDone = 2,
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

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void WindowClosing() override;

  // views::View;
  gfx::Size GetMinimumSize() const override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  ScopedKeepAlive keep_alive_;
  views::WebView* web_view_ = nullptr;
  InitState initialized_ = InitState::kNotInitialized;

  // Not null iff switching to sign-in is in progress.
  base::OnceClosure switch_failure_callback_;

  // Creation time of the picker, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;

  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilePickerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
