// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_
#define COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_

#include <memory>

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}

namespace ui {
class DialogModel;
}

namespace views {
class DialogDelegate;
class WidgetDelegate;
}

namespace web_modal {
class ModalDialogHost;
class WebContentsModalDialogHost;
}

namespace constrained_window {

extern const void* kConstrainedWindowWidgetIdentifier;

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

// As above, but with an owned widget.
std::unique_ptr<views::Widget> ShowWebModalDialogViewsOwned(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents,
    views::Widget::InitParams::Ownership expected_ownership);

// Create a widget for |dialog| that is modal to |web_contents|.
// The modal type of |dialog->GetModalType()| must be
// ui::mojom::ModalType::kChild.
views::Widget* CreateWebModalDialogViews(views::WidgetDelegate* dialog,
                                         content::WebContents* web_contents);

// Create a widget for |dialog| that has a modality given by
// |dialog->GetModalType()|.  The modal type must be either
// ui::mojom::ModalType::kSystem or ui::mojom::ModalType::kWindow.  This places
// the dialog appropriately if |parent| is a valid browser window. Currently,
// |parent| may be null for MODAL_TYPE_WINDOW, but that's a bug and callers
// shouldn't rely on that working. See http://crbug.com/657293. Instead of
// calling this function with null |parent| and MODAL_TYPE_WINDOW, consider
// calling views:: DialogDelegate::CreateDialogWidget(dialog, nullptr, nullptr)
// instead. For dialogs that may appear without direct user interaction (i.e.,
// that may appear while a user is busily accomplishing some other task in the
// browser), consider providing an override of GetDefaultDialogButton on
// |dialog| to suppress the normal behavior of choosing a focused-by-default
// button. This is especially important if the action of the default button has
// consequences on the user's task at hand.
views::Widget* CreateBrowserModalDialogViews(
    std::unique_ptr<views::DialogDelegate> dialog,
    gfx::NativeWindow parent);

// TODO(pbos): Transition calls to this to the unique_ptr<> version above.
views::Widget* CreateBrowserModalDialogViews(views::DialogDelegate* dialog,
                                             gfx::NativeWindow parent);

// Shows a browser-modal dialog based on `dialog_model`, returns pointer
// to the created widget.
views::Widget* ShowBrowserModal(std::unique_ptr<ui::DialogModel> dialog_model,
                                gfx::NativeWindow parent);

// Shows a web/tab-modal dialog based on `dialog_model` and returns its widget.
views::Widget* ShowWebModal(std::unique_ptr<ui::DialogModel> dialog_model,
                            content::WebContents* web_contents);

// True if the platform supports global screen coordinates. This is typically
// supported by most platforms except linux-wayland.
bool SupportsGlobalScreenCoordinates();

// True if the platform clips child widgets to their parent's viewport.
bool PlatformClipsChildrenToViewport();

}  // namespace constrained_window

#endif  // COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_
