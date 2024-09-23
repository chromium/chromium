// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/cast_remoting_connector.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_tracker.h"

class CastToolbarButtonController;
class PrefService;

namespace views {
class Checkbox;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class MediaRemotingDialogCoordinatorViews
    : public MediaRemotingDialogCoordinator {
 public:
  explicit MediaRemotingDialogCoordinatorViews(
      content::WebContents* web_contents);
  MediaRemotingDialogCoordinatorViews(
      const MediaRemotingDialogCoordinatorViews&) = delete;
  MediaRemotingDialogCoordinatorViews& operator=(
      const MediaRemotingDialogCoordinatorViews&) = delete;
  ~MediaRemotingDialogCoordinatorViews() override;

  bool Show(PermissionCallback permission_callback) override;
  void Hide() override;
  bool IsShowing() const override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
  views::ViewTracker tracker_;
};

// Dialog that the user can use to either enable Media Remoting or keep it
// disabled during a Cast session. When Media Remoting is enabled, we send a
// media stream directly to the remote device instead of playing it locally and
// then mirroring it remotely.
class MediaRemotingDialogView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(MediaRemotingDialogView, views::BubbleDialogDelegateView)

 public:
  MediaRemotingDialogView(
      views::View* anchor_view,
      PrefService* pref_service,
      CastToolbarButtonController* action_controller,
      MediaRemotingDialogCoordinator::PermissionCallback permission_callback);
  MediaRemotingDialogView(const MediaRemotingDialogView&) = delete;
  MediaRemotingDialogView& operator=(const MediaRemotingDialogView&) = delete;
  ~MediaRemotingDialogView() override;

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;

  // Runs |permission_callback_| to report whether remoting is allowed by user.
  void ReportPermission(bool allowed);

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<CastToolbarButtonController> action_controller_;
  MediaRemotingDialogCoordinator::PermissionCallback permission_callback_;

  // Checkbox the user can use to indicate whether the preference should be
  // sticky. If this is checked, we record the preference and don't show the
  // dialog again.
  raw_ptr<views::Checkbox> remember_choice_checkbox_ = nullptr;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_
