// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_picker_utils.h"

#include <algorithm>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace {

bool IsExtensionPopupWebContents(content::WebContents* web_contents) {
  return extensions::GetViewType(web_contents) ==
         extensions::mojom::ViewType::kExtensionPopup;
}

// Positions an unparented top-level dialog widget centered over the originating
// window or within the work area of that window's display. This avoids
// platforms defaulting to placing new windows on the primary display (e.g.
// Mac's `-[NSWindow center]` falling back to `[NSScreen mainScreen]`, and
// Windows unpositioned HWNDs defaulting to the primary monitor).
void CenterDialogOnTargetDisplay(views::Widget* widget,
                                 gfx::NativeWindow context,
                                 content::WebContents* web_contents) {
  gfx::NativeWindow window_for_display = context;
  if (!window_for_display && web_contents) {
    window_for_display = web_contents->GetTopLevelNativeWindow();
  }
  if (!window_for_display) {
    return;
  }

  display::Screen* const screen = display::Screen::Get();
  if (!screen) {
    return;
  }

  display::Display display =
      screen->GetDisplayNearestWindow(window_for_display);
  gfx::Rect parent_bounds = display.work_area();
  if (views::Widget* context_widget =
          views::Widget::GetWidgetForNativeWindow(window_for_display)) {
    gfx::Rect context_bounds = context_widget->GetWindowBoundsInScreen();
    if (!context_bounds.IsEmpty()) {
      parent_bounds = context_bounds;
    }
#if BUILDFLAG(IS_MAC)
    // Only enable floating & activation independence if the caller widget was
    // explicitly designed to float.
    const bool is_floating_companion =
        context_widget->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow;

    if (is_floating_companion) {
      widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
      widget->SetActivationIndependence(true);
      widget->SetCanAppearInExistingFullscreenSpaces(true);
    }
#endif
  }

  gfx::Rect dialog_bounds = parent_bounds;
  gfx::Size dialog_size = widget->GetWindowBoundsInScreen().size();
  if (dialog_size.IsEmpty() && widget->non_client_view()) {
    dialog_size = widget->non_client_view()->GetPreferredSize();
  }
  dialog_size.SetToMax(widget->GetMinimumSize());
  if (!dialog_size.IsEmpty()) {
    dialog_bounds.ToCenteredSize(dialog_size);
    // Clamp origin so the dialog remains within the display work area when
    // possible, or anchored at top-left if the display is smaller than the
    // dialog, without shrinking below minimum size.
    const gfx::Rect work_area = display.work_area();
    dialog_bounds.set_x(std::max(
        work_area.x(),
        std::min(dialog_bounds.x(), work_area.right() - dialog_size.width())));
    dialog_bounds.set_y(std::max(
        work_area.y(), std::min(dialog_bounds.y(),
                                work_area.bottom() - dialog_size.height())));
    widget->SetBounds(dialog_bounds);
  }
}

}  // namespace

bool MediaPickerCanShowAsWebModal(content::WebContents* web_contents) {
  return web_contents && !web_contents->IsNeverComposited() &&
         web_modal::WebContentsModalDialogManager::FromWebContents(
             web_contents) &&
         !IsExtensionPopupWebContents(web_contents);
}

views::Widget* CreateMediaPickerDialogWidget(BrowserWindowInterface* browser,
                                             content::WebContents* web_contents,
                                             views::DialogDelegate* delegate,
                                             gfx::NativeWindow context,
                                             gfx::NativeView parent) {
  // If |web_contents| is not a background page then the picker will be shown
  // modal to the web contents. Otherwise, the picker is shown in a separate
  // window.
  views::Widget* widget = nullptr;
  if (MediaPickerCanShowAsWebModal(web_contents)) {
    // Close the extension popup to prevent spoofing.
    if (browser) {
      ExtensionsContainer* container = ExtensionsContainer::From(*browser);
      if (container) {
        container->HideActivePopup();
      }
    }
    widget =
        constrained_window::ShowWebModalDialogViews(delegate, web_contents);
  } else {
    // This becomes a top-level window if it does not have a parent.
    // Top-level windows should usually be draggable.
    if (!parent) {
      delegate->set_draggable(true);
#if BUILDFLAG(IS_CHROMEOS)
      // kSystem is available only on CrOS and should be used here because the
      // dimmer window is shown in front of the media picker dialog when the
      // dialog is requested by an Android app. kSystem brings the dialog in
      // front of the dimmer.
      delegate->SetModalType(ui::mojom::ModalType::kSystem);
#else
      delegate->SetModalType(ui::mojom::ModalType::kNone);
#endif
    }
    widget =
        views::DialogDelegate::CreateDialogWidget(delegate, context, parent);

    if (!parent) {
      CenterDialogOnTargetDisplay(widget, context, web_contents);
    }

    widget->Show();
  }

  return widget;
}
