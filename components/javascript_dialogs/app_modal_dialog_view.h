// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_VIEW_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_VIEW_H_

namespace javascript_dialogs {

class AppModalDialogView {
 public:
  virtual ~AppModalDialogView() {}

  // Shows the dialog.
  virtual void ShowAppModalDialog() = 0;

  // Activates the dialog.
  virtual void ActivateAppModalDialog() = 0;

  // Closes the dialog.
  virtual void CloseAppModalDialog() = 0;

  // Accepts or cancels the dialog.
  virtual void AcceptAppModalDialog() = 0;
  virtual void CancelAppModalDialog() = 0;

  // Should return true when the dialog is being shown.
  virtual bool IsShowing() const = 0;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_VIEW_H_
