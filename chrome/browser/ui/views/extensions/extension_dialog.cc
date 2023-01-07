// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
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
#include "ui/color/color_provider.h"
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

void ExtensionDialog::MaybeFocusRenderer() {
  views::FocusManager* focus_manager = GetWidget()->GetFocusManager();
  DCHECK(focus_manager);

  // Already there's a focused view, so no need to switch the focus.
  if (focus_manager->GetFocusedView())
    return;

  content::RenderWidgetHostView* view = host()->main_frame_host()->GetView();
  if (!view)
    return;

  view->Focus();
}

void ExtensionDialog::SetMinimumContentsSize(int width, int height) {
  extension_view_->SetPreferredSize(gfx::Size(width, height));
}

void ExtensionDialog::OnWindowClosing() {
  if (observer_)
    observer_->ExtensionDialogClosing(this);
}

void ExtensionDialog::HandleCloseExtensionHost(
    extensions::ExtensionHost* host) {
  DCHECK_EQ(host, host_.get());
  GetWidget()->Close();
}

void ExtensionDialog::OnExtensionHostDidStopFirstLoad(
    const extensions::ExtensionHost* host) {
  DCHECK_EQ(host, host_.get());
  // Avoid potential overdraw by removing the temporary background after
  // the extension finishes loading.
  extension_view_->SetBackground(nullptr);
  // The render view is created during the LoadURL(), so we should
  // set the focus to the view if nobody else takes the focus.
  MaybeFocusRenderer();
}

void ExtensionDialog::OnExtensionProcessTerminated(
    const extensions::Extension* extension) {
  if (extension == host_->extension() && observer_)
    observer_->ExtensionTerminated(this);
}

void ExtensionDialog::OnProcessManagerShutdown(
    extensions::ProcessManager* manager) {
  DCHECK(process_manager_observation_.IsObservingSource(manager));
  process_manager_observation_.Reset();
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

  AddRef();
  RegisterDeleteDelegateCallback(
      base::BindOnce(&ExtensionDialog::Release, base::Unretained(this)));
  RegisterWindowClosingCallback(base::BindOnce(
      &ExtensionDialog::OnWindowClosing, base::Unretained(this)));

  extension_host_observation_.Observe(host_.get());
  process_manager_observation_.Observe(
      extensions::ProcessManager::Get(host_->browser_context()));

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowTitle(!init_params.title.empty());
  SetTitle(init_params.title);

  // The base::Unretained() below is safe because this object owns `host_`, so
  // the callback will never fire if `this` is deleted.
  host_->SetCloseHandler(base::BindOnce(
      &ExtensionDialog::HandleCloseExtensionHost, base::Unretained(this)));

  extension_view_ =
      SetContentsView(std::make_unique<ExtensionViewViews>(host_.get()));

  // Show a white background while the extension loads.  This is prettier than
  // flashing a black unfilled window frame.
  extension_view_->SetBackground(
      views::CreateThemedSolidBackground(kColorExtensionDialogBackground));
  extension_view_->SetPreferredSize(init_params.size);
  extension_view_->SetMinimumSize(init_params.min_size);
  extension_view_->SetVisible(true);

  bool can_resize = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Prevent dialog resize mouse cursor in tablet mode, crbug.com/453634.
  if (ash::TabletMode::IsInTabletMode())
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
  const bool should_track_default_frame_colors =
      !(init_params.title_color || init_params.title_inactive_color);
  native_view->SetProperty(chromeos::kTrackDefaultFrameColors,
                           should_track_default_frame_colors);

  if (init_params.title_color) {
    // Frame active color changes the title color when dialog is active.
    native_view->SetProperty(
        chromeos::kFrameActiveColorKey,
        window->GetColorProvider()->GetColor(init_params.title_color.value()));
  }
  if (init_params.title_inactive_color) {
    // Frame inactive color changes the title color when dialog is inactive.
    native_view->SetProperty(chromeos::kFrameInactiveColorKey,
                             window->GetColorProvider()->GetColor(
                                 init_params.title_inactive_color.value()));
  }
#endif

  window->Show();
  // TODO(jamescook): Remove redundant call to Activate()?
  window->Activate();

  // Ensure the DOM JavaScript can respond immediately to keyboard shortcuts.
  host_->host_contents()->Focus();
}
