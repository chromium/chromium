// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include <memory>
#include <utility>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/tablet_mode.h"
#endif

using content::BrowserContext;
using content::WebContents;

ExtensionDialog::ExtensionDialog(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    ExtensionDialogObserver* observer)
    : host_(std::move(host)), observer_(observer) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_use_custom_frame(false);

  AddRef();  // Balanced in DeleteDelegate();

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                 content::Source<BrowserContext>(host_->browser_context()));
  // Listen for the containing view calling window.close();
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<BrowserContext>(host_->browser_context()));
  // Listen for a crash or other termination of the extension process.
  registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::Source<BrowserContext>(host_->browser_context()));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION);
}

ExtensionDialog::~ExtensionDialog() {
}

// static
ExtensionDialog* ExtensionDialog::Show(const GURL& url,
                                       gfx::NativeWindow parent_window,
                                       Profile* profile,
                                       WebContents* web_contents,
                                       bool is_modal,
                                       int width,
                                       int height,
                                       int min_width,
                                       int min_height,
                                       const base::string16& title,
                                       ExtensionDialogObserver* observer) {
  std::unique_ptr<extensions::ExtensionViewHost> host =
      extensions::ExtensionViewHostFactory::CreateDialogHost(url, profile);
  if (!host)
    return NULL;
  // Preferred size must be set before views::Widget::CreateWindowWithParent
  // is called because CreateWindowWithParent refers the result of CanResize().
  ExtensionViewViews* view = GetExtensionView(host.get());
  view->SetPreferredSize(gfx::Size(width, height));
  view->set_minimum_size(gfx::Size(min_width, min_height));
  host->SetAssociatedWebContents(web_contents);

  DCHECK(parent_window);
  extensions::ExtensionViewHost* host_ptr = host.get();
  ExtensionDialog* dialog = new ExtensionDialog(std::move(host), observer);
  dialog->set_title(title);
  dialog->InitWindow(parent_window, is_modal, width, height, min_width,
                     min_height);

  // Show a white background while the extension loads.  This is prettier than
  // flashing a black unfilled window frame.
  view->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  view->SetVisible(true);

  // Ensure the DOM JavaScript can respond immediately to keyboard shortcuts.
  host_ptr->host_contents()->Focus();
  return dialog;
}

void ExtensionDialog::InitWindow(gfx::NativeWindow parent,
                                 bool is_modal,
                                 int width,
                                 int height,
                                 int min_width,
                                 int min_height) {
  views::Widget* window =
      is_modal ? constrained_window::CreateBrowserModalDialogViews(this, parent)
               : views::DialogDelegate::CreateDialogWidget(
                     this, nullptr /* context */, nullptr /* parent */);

  // Center the window over the parent browser window or the screen.
  gfx::Rect screen_rect =
      display::Screen::GetScreen()->GetDisplayNearestWindow(parent).work_area();
  gfx::Rect bounds = parent ? views::Widget::GetWidgetForNativeWindow(parent)
                                  ->GetWindowBoundsInScreen()
                            : screen_rect;
  bounds.ClampToCenteredSize({width, height});

  // Make sure bounds is larger than {min_width, min_height}.
  if (bounds.width() < min_width) {
    bounds.set_x(bounds.x() + (bounds.width() - min_width) / 2);
    bounds.set_width(min_width);
  }
  if (bounds.height() < min_height) {
    bounds.set_y(bounds.y() + (bounds.height() - min_height) / 2);
    bounds.set_height(min_height);
  }

  // Make sure bounds is still on screen.
  bounds.AdjustToFit(screen_rect);
  window->SetBounds(bounds);

  window->Show();
  // TODO(jamescook): Remove redundant call to Activate()?
  window->Activate();
}

ExtensionViewViews* ExtensionDialog::GetExtensionView() const {
  return GetExtensionView(host_.get());
}

ExtensionViewViews* ExtensionDialog::GetExtensionView(
    extensions::ExtensionViewHost* host) {
  return static_cast<ExtensionViewViews*>(host->view());
}

void ExtensionDialog::ObserverDestroyed() {
  observer_ = NULL;
}

void ExtensionDialog::MaybeFocusRenderView() {
  views::FocusManager* focus_manager = GetWidget()->GetFocusManager();
  DCHECK(focus_manager != NULL);

  // Already there's a focused view, so no need to switch the focus.
  if (focus_manager->GetFocusedView())
    return;

  content::RenderWidgetHostView* view =
      host()->render_view_host()->GetWidget()->GetView();
  if (!view)
    return;

  view->Focus();
}

/////////////////////////////////////////////////////////////////////////////
// views::DialogDelegate overrides.

bool ExtensionDialog::CanResize() const {
#if defined(OS_CHROMEOS)
  // Prevent dialog resize mouse cursor in tablet mode, crbug.com/453634.
  if (ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode())
    return false;
#endif
  // Can resize only if minimum contents size set.
  return GetExtensionView()->GetPreferredSize() != gfx::Size();
}

void ExtensionDialog::SetMinimumContentsSize(int width, int height) {
  GetExtensionView()->SetPreferredSize(gfx::Size(width, height));
}

ui::ModalType ExtensionDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool ExtensionDialog::ShouldShowWindowTitle() const {
  return !window_title_.empty();
}

base::string16 ExtensionDialog::GetWindowTitle() const {
  return window_title_;
}

void ExtensionDialog::WindowClosing() {
  if (observer_)
    observer_->ExtensionDialogClosing(this);
}

void ExtensionDialog::DeleteDelegate() {
  // The window has finished closing.  Allow ourself to be deleted.
  Release();
}

views::Widget* ExtensionDialog::GetWidget() {
  return GetExtensionView()->GetWidget();
}

const views::Widget* ExtensionDialog::GetWidget() const {
  return GetExtensionView()->GetWidget();
}

views::View* ExtensionDialog::GetContentsView() {
  return GetExtensionView();
}

/////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides.

void ExtensionDialog::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD:
      // Avoid potential overdraw by removing the temporary background after
      // the extension finishes loading.
      GetExtensionView()->SetBackground(nullptr);
      // The render view is created during the LoadURL(), so we should
      // set the focus to the view if nobody else takes the focus.
      if (content::Details<extensions::ExtensionHost>(host()) == details)
        MaybeFocusRenderView();
      break;
    case extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (content::Details<extensions::ExtensionHost>(host()) != details)
        return;
      GetWidget()->Close();
      break;
    case extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED:
      if (content::Details<extensions::ExtensionHost>(host()) != details)
        return;
      if (observer_)
        observer_->ExtensionTerminated(this);
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
      break;
  }
}
