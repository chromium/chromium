// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

DrivePickerHostController::DrivePickerHostController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {}

DrivePickerHostController::~DrivePickerHostController() {
  ResetControllerState();
}

void DrivePickerHostController::ShowDrivePickerHost(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  // Ensure that we only have one Drive Picker Host view at a time.
  if (view_tracker_.view()) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        std::move(result_handler))
        ->OnError(drive_picker_host::mojom::DrivePickerError::kAlreadyActive);
    return;
  }

  // To ensure the overlay precisely covers the internal area of the browser
  // window (tab strip + toolbar + web contents) and is strictly clipped to the
  // window's edges, we host it as a child view of the BrowserView.
  //
  // This child-view approach is platform-agnostic and avoids the coordinate
  // mismatches or OS-level "bleed-over" (like window shadows) that can occur
  // when using a separate top-level TYPE_POPUP widget.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface_);
  if (!browser_view) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        std::move(result_handler))
        ->OnError(drive_picker_host::mojom::DrivePickerError::kWindowNotFound);
    return;
  }

  auto view = std::make_unique<DrivePickerHostView>(
      browser_window_interface_->GetProfile(), browser_window_interface_);

  // By adding it as a child of BrowserView, it will be drawn on top of all
  // other browser components. We set kViewIgnoredByLayoutKey to true so we can
  // manually manage its bounds to cover the entire BrowserView area.
  view->SetProperty(views::kViewIgnoredByLayoutKey, true);

  DrivePickerHostView* view_ptr = browser_view->AddChildView(std::move(view));
  view_tracker_.SetView(view_ptr);

  // Observe the browser window's widget for resize events to keep the picker
  // overlay perfectly synchronized with the browser's local bounds.
  views::Widget* host_widget = browser_view->GetWidget();
  if (host_widget) {
    browser_window_observation_.Observe(host_widget);
  }

  UpdatePickerViewBounds();

  // Ensure the hosted WebContents is transparent. This allows the WebUI to
  // render its own semi-transparent scrim or floating dialog while the
  // browser window remains partially visible underneath.
  if (view_ptr->GetWebContents()) {
    view_ptr->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  }

  // The controller observes the WebUI's WebContents to be notified when the
  // document load completes, allowing it to trigger the picker logic at the
  // right time.
  Observe(view_ptr->GetWebContents());

  // If the document is already loaded, trigger the picker UI immediately.
  // Otherwise, store the result handler and wait for the document to load.
  if (is_picker_document_loaded_) {
    view_ptr->TriggerDrivePickerHostUi(std::move(result_handler));
  } else {
    pending_picker_result_handler_ = std::move(result_handler);
  }
}

void DrivePickerHostController::ResetControllerState() {
  if (view_tracker_.view()) {
    view_tracker_.view()->parent()->RemoveChildViewT(view_tracker_.view());
  }
  is_picker_document_loaded_ = false;
  pending_picker_result_handler_.reset();
  browser_window_observation_.Reset();
  Observe(nullptr);
}

void DrivePickerHostController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  UpdatePickerViewBounds();
}

void DrivePickerHostController::OnWidgetDestroyed(views::Widget* widget) {
  browser_window_observation_.Reset();
  ResetControllerState();
}

void DrivePickerHostController::UpdatePickerViewBounds() {
  views::View* view = view_tracker_.view();
  if (!view) {
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface_);
  if (browser_view) {
    // Cover the entire local area of BrowserView.
    view->SetBoundsRect(browser_view->GetLocalBounds());
  }
}

void DrivePickerHostController::DocumentOnLoadCompletedInPrimaryMainFrame() {
  is_picker_document_loaded_ = true;

  // We use `pending_picker_result_handler_` to check if there was a request to
  // trigger the picker UI before the document finished loading. This controller
  // only manages a single active picker session at a time.
  if (pending_picker_result_handler_ && view_tracker_.view()) {
    views::AsViewClass<DrivePickerHostView>(view_tracker_.view())
        ->TriggerDrivePickerHostUi(std::move(pending_picker_result_handler_));
  }
}
