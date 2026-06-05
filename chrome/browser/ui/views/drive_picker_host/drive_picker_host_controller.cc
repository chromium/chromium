// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
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
    std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request) {
  // Ensure that we only have one Drive Picker Host view at a time.
  if (picker_widget_) {
    SendErrorToRequest(
        std::move(request),
        drive_picker_host::mojom::DrivePickerError::kAlreadyActive);
    return;
  }

  // To resolve Z-order regressions where popup widgets like the Omnibox
  // dropdown appear on top of the modal overlay, we host the view inside a
  // custom floating views::Widget instead of as a child view of BrowserView.
  //
  // We explicitly set its Z-order to floating, and coordinate bounds are
  // manually synchronized to perfectly overlay the parent window's display.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface_);
  if (!browser_view) {
    SendErrorToRequest(
        std::move(request),
        drive_picker_host::mojom::DrivePickerError::kWindowNotFound);
    return;
  }

  auto view = std::make_unique<DrivePickerHostView>(
      browser_window_interface_->GetProfile(), browser_window_interface_);
  DrivePickerHostView* view_ptr = view.get();

  // Using CLIENT_OWNS_WIDGET, the widget's lifecycle and close state are owned
  // and managed safely by the controller via std::unique_ptr.
  picker_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = browser_view->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "DrivePickerHostWidget";
  picker_widget_->Init(std::move(params));
  picker_widget_->SetContentsView(std::move(view));

  // Set floating window Z-order to keep the hosted picker view on top of other
  // browser elements.
  picker_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  picker_widget_->Show();

  picker_widget_observation_.Observe(picker_widget_.get());

  // Observe the browser window's widget for resize events to keep the picker
  // overlay perfectly synchronized with the browser's local bounds.
  views::Widget* host_widget = browser_view->GetWidget();
  if (host_widget) {
    browser_window_observation_.Observe(host_widget);
  }

  UpdatePickerViewBounds();

  // Explicitly request focus on the newly shown picker view to immediately
  // capture keyboard focus and prevent browser shortcuts from leaking.
  view_ptr->RequestFocus();

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
    view_ptr->TriggerDrivePickerHostUi(std::move(request));
  } else {
    pending_request_ = std::move(request);
  }
}

void DrivePickerHostController::SendErrorToRequest(
    std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request,
    drive_picker_host::mojom::DrivePickerError error) {
  if (request && request->has_result_handler()) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        request->TakeResultHandler())
        ->OnError(error);
  }
}

void DrivePickerHostController::ResetControllerState() {
  picker_widget_observation_.Reset();
  if (picker_widget_) {
    picker_widget_->Close();
    picker_widget_.reset();
  }
  is_picker_document_loaded_ = false;
  pending_request_.reset();
  browser_window_observation_.Reset();
  Observe(nullptr);
}

void DrivePickerHostController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  UpdatePickerViewBounds();
}

void DrivePickerHostController::OnWidgetDestroyed(views::Widget* widget) {
  if (widget == picker_widget_.get()) {
    picker_widget_observation_.Reset();

    // SingleThreadTaskRunner is used here to asynchronously post both the
    // close callback and ResetControllerState().
    //
    // This is because the execution is currently inside the native widget's destruction
    // and notification loop. If ResetControllerState() is synchronously called, it
    // will call picker_widget_.reset(), which immediately deletes the views::Widget
    // C++ object (since CLIENT_OWNS_WIDGET is used). Synchronously deleting the
    // widget object while it is still executing its own destruction callback is
    // unsafe and leads to a Use-After-Free (UAF) or deadlock, freezing the
    // browser. Deferring this to the next event loop tick allows the widget's
    // native destruction sequence to finish safely first.
    if (on_close_callback_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_close_callback_));
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DrivePickerHostController::ResetControllerState,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    browser_window_observation_.Reset();
    ResetControllerState();
  }
}

void DrivePickerHostController::UpdatePickerViewBounds() {
  if (!picker_widget_) {
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface_);
  if (browser_view) {
    picker_widget_->SetBounds(browser_view->GetBoundsInScreen());
  }
}

void DrivePickerHostController::DocumentOnLoadCompletedInPrimaryMainFrame() {
  is_picker_document_loaded_ = true;

  // We use `pending_request_` to check if there was a request to
  // trigger the picker UI before the document finished loading. This controller
  // only manages a single active picker session at a time.
  if (pending_request_ && picker_widget_) {
    views::AsViewClass<DrivePickerHostView>(picker_widget_->GetContentsView())
        ->TriggerDrivePickerHostUi(std::move(pending_request_));
  }
}
