// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAVE_TO_DRIVE_ADD_ACCOUNT_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_SAVE_TO_DRIVE_ADD_ACCOUNT_DIALOG_CONTROLLER_H_

#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

namespace save_to_drive {

// This class is responsible for showing and hiding the add account dialog
// pop-up window in the Save to Drive Account Chooser flow.
class AddAccountDialogController : public content::WebContentsObserver {
 public:
  explicit AddAccountDialogController(content::WebContents* web_contents);
  AddAccountDialogController(const AddAccountDialogController&) = delete;
  AddAccountDialogController& operator=(const AddAccountDialogController&) =
      delete;
  ~AddAccountDialogController() override;

  // Closes the dialog if it is showing.
  void Close();
  // Shows the dialog if it is not showing.
  void Show();

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  // Resizes and focuses the add account dialog pop-up window.
  void ResizeAndFocusPopupWindow();

  raw_ptr<content::WebContents> source_window_ = nullptr;
  raw_ptr<content::WebContents> popup_window_ = nullptr;
};
}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_SAVE_TO_DRIVE_ADD_ACCOUNT_DIALOG_CONTROLLER_H_
