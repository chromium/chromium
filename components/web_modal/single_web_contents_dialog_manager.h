// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_SINGLE_WEB_CONTENTS_DIALOG_MANAGER_H_
#define COMPONENTS_WEB_MODAL_SINGLE_WEB_CONTENTS_DIALOG_MANAGER_H_

#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_modal {

class WebContentsModalDialogHost;

// Interface from SingleWebContentsDialogManager to
// WebContentsModalDialogManager.
class SingleWebContentsDialogManagerDelegate {
 public:
  SingleWebContentsDialogManagerDelegate() = default;

  SingleWebContentsDialogManagerDelegate(
      const SingleWebContentsDialogManagerDelegate&) = delete;
  SingleWebContentsDialogManagerDelegate& operator=(
      const SingleWebContentsDialogManagerDelegate&) = delete;

  virtual ~SingleWebContentsDialogManagerDelegate() = default;

  virtual content::WebContents* GetWebContents() const = 0;

  // Notify the delegate that the dialog is closing. The native
  // manager will be deleted before the end of this call.
  virtual void WillClose(gfx::NativeWindow dialog) = 0;
};

// Provides an interface for platform-specific UI implementation for the web
// contents modal dialog. Each object will manage a single dialog window
// during its lifecycle.
//
// Implementation classes should accept a dialog window at construction time
// and register to be notified when the dialog is closing, so that it can
// notify its delegate (WillClose method).
class SingleWebContentsDialogManager {
 public:
  SingleWebContentsDialogManager(const SingleWebContentsDialogManager&) =
      delete;
  SingleWebContentsDialogManager& operator=(
      const SingleWebContentsDialogManager&) = delete;

  virtual ~SingleWebContentsDialogManager() = default;

  // Makes the web contents modal dialog visible. Only one web contents modal
  // dialog is shown at a time per tab.
  virtual void Show() = 0;

  // Hides the web contents modal dialog without closing it.
  virtual void Hide() = 0;

  // Closes the web contents modal dialog.
  // If this method causes a WillClose() call to the delegate, the manager
  // will be deleted at the close of that invocation.
  virtual void Close() = 0;

  // Sets focus on the web contents modal dialog.
  virtual void Focus() = 0;

  // Runs a pulse animation for the web contents modal dialog.
  virtual void Pulse() = 0;

  // Called when the host view for the dialog has changed.
  virtual void HostChanged(WebContentsModalDialogHost* new_host) = 0;

  // Return the dialog under management by this object.
  virtual gfx::NativeWindow dialog() = 0;

  // Returns true if the web contents modal dialog is the currently active
  // window, otherwise false.
  virtual bool IsActive() const = 0;

 protected:
  SingleWebContentsDialogManager() = default;
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_SINGLE_WEB_CONTENTS_DIALOG_MANAGER_H_
