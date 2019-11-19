// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/constrained_window/constrained_window_views.h"

#include <algorithm>
#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/constrained_window/constrained_window_views_client.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_MACOSX)
#import "components/constrained_window/native_web_contents_modal_dialog_manager_views_mac.h"
#endif

using web_modal::ModalDialogHost;
using web_modal::ModalDialogHostObserver;

namespace constrained_window {
namespace {

// Storage access for the currently active ConstrainedWindowViewsClient.
std::unique_ptr<ConstrainedWindowViewsClient>& CurrentClient() {
  static base::NoDestructor<std::unique_ptr<ConstrainedWindowViewsClient>>
      client;
  return *client;
}

// The name of a key to store on the window handle to associate
// WidgetModalDialogHostObserverViews with the Widget.
const char* const kWidgetModalDialogHostObserverViewsKey =
    "__WIDGET_MODAL_DIALOG_HOST_OBSERVER_VIEWS__";

// Applies positioning changes from the ModalDialogHost to the Widget.
class WidgetModalDialogHostObserverViews
    : public views::WidgetObserver,
      public ModalDialogHostObserver {
 public:
  WidgetModalDialogHostObserverViews(ModalDialogHost* host,
                                     views::Widget* target_widget,
                                     const char *const native_window_property)
      : host_(host),
        target_widget_(target_widget),
        native_window_property_(native_window_property) {
    DCHECK(host_);
    DCHECK(target_widget_);
    host_->AddObserver(this);
    target_widget_->AddObserver(this);
  }

  ~WidgetModalDialogHostObserverViews() override {
    if (host_)
      host_->RemoveObserver(this);
    target_widget_->RemoveObserver(this);
    target_widget_->SetNativeWindowProperty(native_window_property_, nullptr);
  }

  // WidgetObserver overrides
  void OnWidgetDestroying(views::Widget* widget) override { delete this; }

  // WebContentsModalDialogHostObserver overrides
  void OnPositionRequiresUpdate() override {
    UpdateWidgetModalDialogPosition(target_widget_, host_);
  }

  void OnHostDestroying() override {
    host_->RemoveObserver(this);
    host_ = nullptr;
  }

 private:
  ModalDialogHost* host_;
  views::Widget* target_widget_;
  const char* const native_window_property_;

  DISALLOW_COPY_AND_ASSIGN(WidgetModalDialogHostObserverViews);
};

void UpdateModalDialogPosition(views::Widget* widget,
                               web_modal::ModalDialogHost* dialog_host,
                               const gfx::Size& size) {
  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget->HasCapture())
    return;

  views::Widget* host_widget =
      views::Widget::GetWidgetForNativeView(dialog_host->GetHostView());

  // If the host view is not backed by a Views::Widget, just update the widget
  // size. This can happen on MacViews under the Cocoa browser where the window
  // modal dialogs are displayed as sheets, and their position is managed by a
  // ConstrainedWindowSheetController instance.
  if (!host_widget) {
    widget->SetSize(size);
    return;
  }

  gfx::Point position = dialog_host->GetDialogPosition(size);
  views::Border* border = widget->non_client_view()->frame_view()->border();
  // Border may be null during widget initialization.
  if (border) {
    // Align the first row of pixels inside the border. This is the apparent
    // top of the dialog.
    position.set_y(position.y() - border->GetInsets().top());
  }

  if (widget->is_top_level()) {
    position += host_widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
    // If the dialog extends partially off any display, clamp its position to
    // be fully visible within that display. If the dialog doesn't intersect
    // with any display clamp its position to be fully on the nearest display.
    gfx::Rect display_rect = gfx::Rect(position, size);
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            dialog_host->GetHostView());
    const gfx::Rect work_area = display.work_area();
    if (!work_area.Contains(display_rect))
      display_rect.AdjustToFit(work_area);
    position = display_rect.origin();
  }

  widget->SetBounds(gfx::Rect(position, size));
}

}  // namespace

// static
void SetConstrainedWindowViewsClient(
    std::unique_ptr<ConstrainedWindowViewsClient> new_client) {
  CurrentClient() = std::move(new_client);
}

void UpdateWebContentsModalDialogPosition(
    views::Widget* widget,
    web_modal::WebContentsModalDialogHost* dialog_host) {
  gfx::Size size = widget->GetRootView()->GetPreferredSize();
  gfx::Size max_size = dialog_host->GetMaximumDialogSize();
  // Enlarge the max size by the top border, as the dialog will be shifted
  // outside the area specified by the dialog host by this amount later.
  views::Border* border =
      widget->non_client_view()->frame_view()->border();
  // Border may be null during widget initialization.
  if (border)
    max_size.Enlarge(0, border->GetInsets().top());
  size.SetToMin(max_size);
  UpdateModalDialogPosition(widget, dialog_host, size);
}

void UpdateWidgetModalDialogPosition(views::Widget* widget,
                                     web_modal::ModalDialogHost* dialog_host) {
  UpdateModalDialogPosition(widget, dialog_host,
                            widget->GetRootView()->GetPreferredSize());
}

content::WebContents* GetTopLevelWebContents(
    content::WebContents* initiator_web_contents) {
  return guest_view::GuestViewBase::GetTopLevelWebContents(
      initiator_web_contents);
}

views::Widget* ShowWebModalDialogViews(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents) {
  DCHECK(CurrentClient());
  // For embedded WebContents, use the embedder's WebContents for constrained
  // window.
  content::WebContents* web_contents =
      GetTopLevelWebContents(initiator_web_contents);
  views::Widget* widget = CreateWebModalDialogViews(dialog, web_contents);
  ShowModalDialog(widget->GetNativeWindow(), web_contents);
  return widget;
}

#if defined(OS_MACOSX)
views::Widget* ShowWebModalDialogWithOverlayViews(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents) {
  DCHECK(CurrentClient());
  // For embedded WebContents, use the embedder's WebContents for constrained
  // window.
  content::WebContents* web_contents =
      GetTopLevelWebContents(initiator_web_contents);
  views::Widget* widget = CreateWebModalDialogViews(dialog, web_contents);
  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  std::unique_ptr<web_modal::SingleWebContentsDialogManager> dialog_manager(
      new NativeWebContentsModalDialogManagerViewsMac(widget->GetNativeWindow(),
                                                      manager));
  manager->ShowDialogWithManager(widget->GetNativeWindow(),
                                 std::move(dialog_manager));
  return widget;
}
#endif

views::Widget* CreateWebModalDialogViews(views::WidgetDelegate* dialog,
                                         content::WebContents* web_contents) {
  DCHECK_EQ(ui::MODAL_TYPE_CHILD, dialog->GetModalType());
  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  CHECK(manager);
  return views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr,
      manager->delegate()->GetWebContentsModalDialogHost()->GetHostView());
}

views::Widget* CreateBrowserModalDialogViews(views::DialogDelegate* dialog,
                                             gfx::NativeWindow parent) {
  DCHECK_NE(ui::MODAL_TYPE_CHILD, dialog->GetModalType());
  DCHECK_NE(ui::MODAL_TYPE_NONE, dialog->GetModalType());
  DCHECK(!parent || CurrentClient());

  gfx::NativeView parent_view =
      parent ? CurrentClient()->GetDialogHostView(parent) : nullptr;
  views::Widget* widget =
      views::DialogDelegate::CreateDialogWidget(dialog, nullptr, parent_view);

  bool requires_positioning = dialog->use_custom_frame();

#if defined(OS_MACOSX)
  // On Mac, window modal dialogs are displayed as sheets, so their position is
  // managed by the parent window.
  requires_positioning = false;
#endif

  if (!requires_positioning)
    return widget;

  ModalDialogHost* host =
      parent ? CurrentClient()->GetModalDialogHost(parent) : nullptr;
  if (host) {
    DCHECK_EQ(parent_view, host->GetHostView());
    ModalDialogHostObserver* dialog_host_observer =
        new WidgetModalDialogHostObserverViews(
            host, widget, kWidgetModalDialogHostObserverViewsKey);
    dialog_host_observer->OnPositionRequiresUpdate();
  }
  return widget;
}

}  // namespace constrained window
