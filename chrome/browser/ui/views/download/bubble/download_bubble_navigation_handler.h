// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_NAVIGATION_HANDLER_H_

#include "chrome/browser/download/download_ui_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace offline_items_collection {
struct ContentId;
}

class DownloadBubbleNavigationHandler {
 public:
  using CloseOnDeactivatePin =
      views::BubbleDialogDelegate::CloseOnDeactivatePin;

  // Primary dialog is either main or partial view.
  virtual void OpenPrimaryDialog() = 0;

  // Opens the security dialog. If the bubble is not currently open, it creates
  // a new bubble to do so.
  virtual void OpenSecurityDialog(
      const offline_items_collection::ContentId& content_id) = 0;

  virtual void CloseDialog(views::Widget::ClosedReason reason) = 0;

  // Callback invoked when the dialog has been interacted with by hovering over
  // or by focusing (on the partial view).
  virtual void OnDialogInteracted() = 0;

  virtual void OnSecurityDialogButtonPress(
      const DownloadUIModel& model,
      DownloadCommands::Command command) = 0;

  // Returns a CloseOnDeactivatePin for the download bubble. For the lifetime of
  // the returned pin (if non-null), the download bubble will not close on
  // deactivate. Returns nullptr if the bubble is not open.
  virtual std::unique_ptr<CloseOnDeactivatePin>
  PreventDialogCloseOnDeactivate() = 0;

  virtual base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_NAVIGATION_HANDLER_H_
