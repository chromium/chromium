// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_
#define COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_

#include <memory>

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}

namespace views {
class DialogDelegate;
class Widget;
class WidgetDelegate;
}

namespace web_modal {
class ModalDialogHost;
class WebContentsModalDialogHost;
}

namespace constrained_window {

class ConstrainedWindowViewsClient;

// Sets the ConstrainedWindowClient impl.
void SetConstrainedWindowViewsClient(
    std::unique_ptr<ConstrainedWindowViewsClient> client);

// Update the position of dialog |widget| against |dialog_host|. This is used to
// reposition widgets e.g. when the host dimensions change.
void UpdateWebContentsModalDialogPosition(
    views::Widget* widget,
    web_modal::WebContentsModalDialogHost* dialog_host);

void UpdateWidgetModalDialogPosition(
    views::Widget* widget,
    web_modal::ModalDialogHost* dialog_host);

// Returns the top level WebContents of |initiator_web_contents|.
content::WebContents* GetTopLevelWebContents(
    content::WebContents* initiator_web_contents);

// Shows the dialog with a new SingleWebContentsDialogManager. The dialog will
// notify via WillClose() when it is being destroyed.
void ShowModalDialog(gfx::NativeWindow dialog,
                     content::WebContents* web_contents);

// Calls CreateWebModalDialogViews, shows the dialog, and returns its widget.
views::Widget* ShowWebModalDialogViews(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents);

#if defined(OS_MACOSX)
// Like ShowWebModalDialogViews, but used to show a native dialog "sheet" on
// Mac. Sheets are always modal to their parent window. To make them tab-modal,
// this provides an invisible tab-modal overlay window managed by
// WebContentsModalDialogManager, which can host a dialog sheet.
views::Widget* ShowWebModalDialogWithOverlayViews(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents);
#endif

// Create a widget for |dialog| that is modal to |web_contents|.
// The modal type of |dialog->GetModalType()| must be ui::MODAL_TYPE_CHILD.
views::Widget* CreateWebModalDialogViews(views::WidgetDelegate* dialog,
                                         content::WebContents* web_contents);

// Create a widget for |dialog| that has a modality given by
// |dialog->GetModalType()|.  The modal type must be either
// ui::MODAL_TYPE_SYSTEM or ui::MODAL_TYPE_WINDOW.  This places the dialog
// appropriately if |parent| is a valid browser window. Currently, |parent| may
// be null for MODAL_TYPE_WINDOW, but that's a bug and callers shouldn't rely on
// that working. See http://crbug.com/657293.
// For dialogs that may appear without direct user interaction (i.e., that may
// appear while a user is busily accomplishing some other task in the browser),
// consider providing an override of GetDefaultDialogButton on |dialog| to
// suppress the normal behavior of choosing a focused-by-default button. This is
// especially important if the action of the default button has consequences on
// the user's task at hand.
views::Widget* CreateBrowserModalDialogViews(views::DialogDelegate* dialog,
                                             gfx::NativeWindow parent);

}  // namespace constrained_window

#endif  // COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_
