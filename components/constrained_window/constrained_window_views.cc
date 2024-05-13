// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/constrained_window/constrained_window_views.h"
#include "base/memory/raw_ptr.h"

#include <algorithm>
#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/constrained_window/constrained_window_views_client.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

using web_modal::ModalDialogHost;
using web_modal::ModalDialogHostObserver;

namespace constrained_window {

const void* kConstrainedWindowWidgetIdentifier = "ConstrainedWindowWidget";

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
class WidgetModalDialogHostObserverViews : public views::WidgetObserver,
                                           public ModalDialogHostObserver {
 public:
  WidgetModalDialogHostObserverViews(ModalDialogHost* host,
                                     views::Widget* target_widget,
                                     const char* const native_window_property)
      : host_(host),
        target_widget_(target_widget),
        native_window_property_(native_window_property) {
    DCHECK(host_);
    DCHECK(target_widget_);
    host_->AddObserver(this);
    target_widget_->AddObserver(this);
  }

  WidgetModalDialogHostObserverViews(
      const WidgetModalDialogHostObserverViews&) = delete;
  WidgetModalDialogHostObserverViews& operator=(
      const WidgetModalDialogHostObserverViews&) = delete;

  ~WidgetModalDialogHostObserverViews() override {
    if (host_)
      host_->RemoveObserver(this);
    target_widget_->RemoveObserver(this);
    target_widget_->SetNativeWindowProperty(native_window_property_, nullptr);
    CHECK(!IsInObserverList());
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
  raw_ptr<ModalDialogHost> host_;
  raw_ptr<views::Widget> target_widget_;
  const char* const native_window_property_;
};

gfx::Rect GetModalDialogBounds(views::Widget* widget,
                               web_modal::ModalDialogHost* dialog_host,
                               const gfx::Size& size) {
  views::Widget* const host_widget =
      views::Widget::GetWidgetForNativeView(dialog_host->GetHostView());
  CHECK(host_widget);

  gfx::Point position = dialog_host->GetDialogPosition(size);
  // Align the first row of pixels inside the border. This is the apparent top
  // of the dialog.
  position.set_y(position.y() -
                 widget->non_client_view()->frame_view()->GetInsets().top());

  gfx::Rect dialog_bounds(position, size);

  if (widget->is_top_level() && SupportsGlobalScreenCoordinates()) {
    gfx::Rect dialog_screen_bounds =
        dialog_bounds +
        host_widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
    const gfx::Rect host_screen_bounds = host_widget->GetWindowBoundsInScreen();

    // TODO(crbug.com/40851111): The requested dialog bounds should never fall
    // outside the bounds of the transient parent.
    DCHECK(dialog_screen_bounds.Intersects(host_screen_bounds));

    // Adjust the dialog bound to ensure it remains visible on the display.
    const gfx::Rect display_work_area =
        display::Screen::GetScreen()
            ->GetDisplayNearestView(dialog_host->GetHostView())
            .work_area();
    if (!display_work_area.Contains(dialog_screen_bounds)) {
      dialog_screen_bounds.AdjustToFit(display_work_area);
    }

    // For platforms that clip transient children to the viewport we must
    // maximize its bounds on the display whilst keeping it within the host
    // bounds to avoid viewport clipping.
    // In the case that the host window bounds do not have sufficient overlap
    // with the display, and the dialog cannot be shown in its entirety, this is
    // a recoverable state as users are still able to reposition the host window
    // back onto the display.
    if (PlatformClipsChildrenToViewport() &&
        !host_screen_bounds.Contains(dialog_screen_bounds)) {
      dialog_screen_bounds.AdjustToFit(host_screen_bounds);
    }

    // Readjust the position of the dialog.
    dialog_bounds.set_origin(dialog_screen_bounds.origin());
  }
  return dialog_bounds;
}

void UpdateModalDialogPosition(views::Widget* widget,
                               web_modal::ModalDialogHost* dialog_host,
                               const gfx::Size& size) {
  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget->HasCapture()) {
    return;
  }

  views::Widget* const host_widget =
      views::Widget::GetWidgetForNativeView(dialog_host->GetHostView());

  // If the host view is not backed by a Views::Widget, just update the widget
  // size. This can happen on MacViews under the Cocoa browser where the window
  // modal dialogs are displayed as sheets, and their position is managed by a
  // ConstrainedWindowSheetController instance.
  if (!host_widget) {
    widget->SetSize(size);
    return;
  }

  widget->SetBounds(GetModalDialogBounds(widget, dialog_host, size));
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
  gfx::Size size = widget->GetRootView()->GetPreferredSize({});
  gfx::Size max_size = dialog_host->GetMaximumDialogSize();
  // Enlarge the max size by the top border, as the dialog will be shifted
  // outside the area specified by the dialog host by this amount later.
  max_size.Enlarge(0,
                   widget->non_client_view()->frame_view()->GetInsets().top());
  size.SetToMin(max_size);
  UpdateModalDialogPosition(widget, dialog_host, size);
}

void UpdateWidgetModalDialogPosition(views::Widget* widget,
                                     web_modal::ModalDialogHost* dialog_host) {
  UpdateModalDialogPosition(widget, dialog_host,
                            widget->GetRootView()->GetPreferredSize({}));
}

content::WebContents* GetTopLevelWebContents(
    content::WebContents* initiator_web_contents) {
  // TODO(mcnee): While calling both `GetResponsibleWebContents` and
  // `GetTopLevelWebContents` appears redundant, there appears to still be cases
  // where users of guest view are not initializing the guest WebContents
  // properly, causing GetResponsibleWebContents to break. See
  // https://crbug.com/1325850
  // The order of composing these methods is arbitrary.
  return guest_view::GuestViewBase::GetTopLevelWebContents(
      initiator_web_contents->GetResponsibleWebContents());
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

std::unique_ptr<views::Widget> ShowWebModalDialogViewsOwned(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents) {
  views::Widget* widget =
      ShowWebModalDialogViews(dialog, initiator_web_contents);
  CHECK_EQ(widget->ownership(),
           views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  return base::WrapUnique<views::Widget>(widget);
}

views::Widget* CreateWebModalDialogViews(views::WidgetDelegate* dialog,
                                         content::WebContents* web_contents) {
  DCHECK_EQ(ui::MODAL_TYPE_CHILD, dialog->GetModalType());
  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);

  // TODO(http://crbug/1273287): Drop "if" and DEBUG_ALIAS_FOR_GURL after fix.
  if (!manager) {
    const GURL& url = web_contents->GetLastCommittedURL();
    DEBUG_ALIAS_FOR_GURL(url_alias, url);
    LOG_IF(FATAL, !manager)
        << "CreateWebModalDialogViews without a manager"
        << ", scheme=" << url.scheme_piece() << ", host=" << url.host_piece();
  }

  web_modal::ModalDialogHost* dialog_host =
      manager->delegate()->GetWebContentsModalDialogHost();
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr, dialog_host->GetHostView());
  dialog->set_desired_bounds_delegate(base::BindRepeating(
      [](views::Widget* widget,
         web_modal::ModalDialogHost* dialog_host) -> gfx::Rect {
        return GetModalDialogBounds(
            widget, dialog_host, widget->GetRootView()->GetPreferredSize({}));
      },
      widget, manager->delegate()->GetWebContentsModalDialogHost()));
  widget->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(kConstrainedWindowWidgetIdentifier));

  return widget;
}

views::Widget* CreateBrowserModalDialogViews(
    std::unique_ptr<views::DialogDelegate> dialog,
    gfx::NativeWindow parent) {
  return CreateBrowserModalDialogViews(dialog.release(), parent);
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
  widget->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(kConstrainedWindowWidgetIdentifier));

  bool requires_positioning = dialog->use_custom_frame();

#if BUILDFLAG(IS_APPLE)
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

views::Widget* ShowBrowserModal(std::unique_ptr<ui::DialogModel> dialog_model,
                                gfx::NativeWindow parent) {
  // TODO(crbug.com/41493925): Remove will_use_custom_frame once native frame
  // dialogs support autosize.
  bool will_use_custom_frame = views::DialogDelegate::CanSupportCustomFrame(
      parent ? CurrentClient()->GetDialogHostView(parent) : nullptr);
  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::MODAL_TYPE_WINDOW, will_use_custom_frame);
  dialog->SetOwnedByWidget(true);
  auto* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog), parent);
  CHECK_EQ(widget->widget_delegate()->AsDialogDelegate()->use_custom_frame(),
           will_use_custom_frame);
  widget->Show();
  return widget;
}

views::Widget* ShowWebModal(std::unique_ptr<ui::DialogModel> dialog_model,
                            content::WebContents* web_contents) {
  return constrained_window::ShowWebModalDialogViews(
      views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                ui::MODAL_TYPE_CHILD)
          .release(),
      web_contents);
}

bool SupportsGlobalScreenCoordinates() {
#if !BUILDFLAG(IS_OZONE)
  return true;
#else
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .supports_global_screen_coordinates;
#endif
}

bool PlatformClipsChildrenToViewport() {
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  return true;
#else
  return false;
#endif
}

}  // namespace constrained_window
