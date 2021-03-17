// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

class MediaRouterActionController;
class PrefService;

namespace views {
class Checkbox;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

// Dialog that the user can use to either enable Media Remoting or keep it
// disabled during a Cast session. When Media Remoting is enabled, we send a
// media stream directly to the remote device instead of playing it locally and
// then mirroring it remotely.
class MediaRemotingDialogView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(MediaRemotingDialogView);
  MediaRemotingDialogView(const MediaRemotingDialogView&) = delete;
  MediaRemotingDialogView& operator=(const MediaRemotingDialogView&) = delete;

  using PermissionCallback = base::OnceCallback<void(bool)>;
  // Checks the existing preference value, and if unset, instantiates and shows
  // the singleton dialog to get user's permission. |callback| runs on the same
  // stack if the preference was set, or asynchronously when user sets the
  // permission through the dialog.
  static void GetPermission(content::WebContents* web_contents,
                            PermissionCallback callback);

  // No-op if the dialog is currently not shown.
  static void HideDialog();

  static bool IsShowing();

 private:
  MediaRemotingDialogView(views::View* anchor_view,
                          PrefService* pref_service,
                          MediaRouterActionController* action_controller,
                          PermissionCallback callback);
  ~MediaRemotingDialogView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;

  // Runs |permission_callback_| to report whether remoting is allowed by user.
  void ReportPermission(bool allowed);

  // The singleton dialog instance. This is a nullptr when a dialog is not
  // shown.
  static MediaRemotingDialogView* instance_;

  PermissionCallback permission_callback_;

  PrefService* const pref_service_;
  MediaRouterActionController* const action_controller_;

  // Checkbox the user can use to indicate whether the preference should be
  // sticky. If this is checked, we record the preference and don't show the
  // dialog again.
  views::Checkbox* remember_choice_checkbox_ = nullptr;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_
