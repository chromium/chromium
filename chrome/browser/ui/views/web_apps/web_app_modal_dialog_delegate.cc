// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_modal_dialog_delegate.h"

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "ui/views/widget/widget.h"

namespace web_app {

WebAppModalDialogDelegate::WebAppModalDialogDelegate(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebAppModalDialogDelegate::~WebAppModalDialogDelegate() = default;

void WebAppModalDialogDelegate::OnWidgetShownStartTracking(
    views::Widget* dialog_widget) {
  occlusion_observation_.Observe(dialog_widget);
  widget_observation_.Observe(dialog_widget);
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(
      dialog_widget);
}

void WebAppModalDialogDelegate::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    CloseDialogAsIgnored();
  }
}

void WebAppModalDialogDelegate::WebContentsDestroyed() {
  CloseDialogAsIgnored();
}

void WebAppModalDialogDelegate::PrimaryPageChanged(content::Page& page) {
  CloseDialogAsIgnored();
}

void WebAppModalDialogDelegate::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
}

void WebAppModalDialogDelegate::OnOcclusionStateChanged(bool occluded) {
  // If a picture-in-picture window is occluding the dialog, force it to close
  // to prevent spoofing.
  if (occluded) {
    PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
  }
}

}  // namespace web_app
