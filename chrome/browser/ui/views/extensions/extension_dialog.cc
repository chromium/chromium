// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include <memory>
#include <utility>

#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/tablet_mode.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#endif

ExtensionDialog::InitParams::InitParams(gfx::Size size)
    : size(std::move(size)) {}
ExtensionDialog::InitParams::InitParams(const InitParams& other) = default;
ExtensionDialog::InitParams::~InitParams() = default;

// static
ExtensionDialog* ExtensionDialog::Show(const GURL& url,
                                       gfx::NativeWindow parent_window,
                                       Profile* profile,
                                       content::WebContents* web_contents,
                                       ExtensionDialogObserver* observer,
                                       const InitParams& init_params) {
  DCHECK(parent_window);

  std::unique_ptr<extensions::ExtensionViewHost> host =
      extensions::ExtensionViewHostFactory::CreateDialogHost(url, profile);
  if (!host)
    return nullptr;
  host->SetAssociatedWebContents(web_contents);

  return new ExtensionDialog(std::move(host), observer, parent_window,
                             init_params);
}

void ExtensionDialog::ObserverDestroyed() {
  observer_ = nullptr;
}

void ExtensionDialog::MaybeFocusRenderView() {
  views::FocusManager* focus_manager = GetWidget()->GetFocusManager();
  DCHECK(focus_manager);

  // Already there's a focused view, so no need to switch the focus.
  if (focus_manager->GetFocusedView())
    return;

  content::RenderWidgetHostView* view =
      host()->render_view_host()->GetWidget()->GetView();
  if (!view)
    return;

  view->Focus();
}

void ExtensionDialog::SetMinimumContentsSize(int width, int height) {
  extension_view_->SetPreferredSize(gfx::Size(width, height));
}

ui::ModalType ExtensionDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ExtensionDialog::WindowClosing() {
  if (observer_)
    observer_->ExtensionDialogClosing(this);
}

void ExtensionDialog::DeleteDelegate() {
  // The window has finished closing.  Allow ourself to be deleted.
  Release();
}

// TODO(ellyjones): Are either of these overrides necessary? It seems like
// extension_view_ is always this dialog's contents view, in which case
// GetWidget will already behave this way.
views::Widget* ExtensionDialog::GetWidget() {
  return extension_view_ ? extension_view_->GetWidget() : nullptr;
}

const views::Widget* ExtensionDialog::GetWidget() const {
  return extension_view_ ? extension_view_->GetWidget() : nullptr;
}

views::View* ExtensionDialog::GetContentsView() {
  if (!extension_view_) {
    extension_view_ = new ExtensionViewViews(host_.get());  // Owned by caller.

    // Show a white background while the extension loads.  This is prettier than
    // flashing a black unfilled window frame.
    extension_view_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  }

  return extension_view_;
}

void ExtensionDialog::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD:
      // Avoid potential overdraw by removing the temporary background after
      // the extension finishes loading.
      extension_view_->SetBackground(nullptr);
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

ExtensionDialog::~ExtensionDialog() = default;

ExtensionDialog::ExtensionDialog(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    ExtensionDialogObserver* observer,
    gfx::NativeWindow parent_window,
    const InitParams& init_params)
    : host_(std::move(host)), observer_(observer) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_use_custom_frame(false);

  AddRef();  // Balanced in DeleteDelegate();

  const content::Source<content::BrowserContext> source =
      host_->browser_context();
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                 source);
  // Listen for the containing view calling window.close();
  registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE, source);
  // Listen for a crash or other termination of the extension process.
  registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 source);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION);

  SetShowTitle(!init_params.title.empty());
  SetTitle(init_params.title);

  bool can_resize = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Prevent dialog resize mouse cursor in tablet mode, crbug.com/453634.
  if (ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode())
    can_resize = false;
#endif
  SetCanResize(can_resize);

  views::Widget* window =
      init_params.is_modal
          ? constrained_window::CreateBrowserModalDialogViews(this,
                                                              parent_window)
          : views::DialogDelegate::CreateDialogWidget(this, nullptr, nullptr);

  // Center the window over the parent browser window or the screen.
  gfx::Rect screen_rect = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(parent_window)
                              .work_area();
  gfx::Rect bounds = screen_rect;
  if (parent_window) {
    views::Widget* parent_widget =
        views::Widget::GetWidgetForNativeWindow(parent_window);
    if (parent_widget)
      bounds = parent_widget->GetWindowBoundsInScreen();
  }
  bounds.ClampToCenteredSize(init_params.size);

  // Make sure bounds is larger than {min_size}.
  if (bounds.width() < init_params.min_size.width()) {
    bounds.set_x(bounds.x() +
                 (bounds.width() - init_params.min_size.width()) / 2);
    bounds.set_width(init_params.min_size.width());
  }
  if (bounds.height() < init_params.min_size.height()) {
    bounds.set_y(bounds.y() +
                 (bounds.height() - init_params.min_size.height()) / 2);
    bounds.set_height(init_params.min_size.height());
  }

  // Make sure bounds is still on screen.
  bounds.AdjustToFit(screen_rect);
  window->SetBounds(bounds);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  aura::Window* native_view = window->GetNativeWindow();
  if (init_params.title_color) {
    // Frame active color changes the title color when dialog is active.
    native_view->SetProperty(chromeos::kFrameActiveColorKey,
                             init_params.title_color.value());
  }
  if (init_params.title_inactive_color) {
    // Frame inactive color changes the title color when dialog is inactive.
    native_view->SetProperty(chromeos::kFrameInactiveColorKey,
                             init_params.title_inactive_color.value());
  }
#endif

  window->Show();
  // TODO(jamescook): Remove redundant call to Activate()?
  window->Activate();

  // Creating the Widget should have called GetContentsView() and created
  // |extension_view_|.
  DCHECK(extension_view_);
  extension_view_->SetPreferredSize(init_params.size);
  extension_view_->set_minimum_size(init_params.min_size);
  extension_view_->SetVisible(true);

  // Ensure the DOM JavaScript can respond immediately to keyboard shortcuts.
  host_->host_contents()->Focus();
}
