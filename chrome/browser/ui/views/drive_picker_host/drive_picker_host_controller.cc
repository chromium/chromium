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
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

DrivePickerHostController::DrivePickerHostController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {}

DrivePickerHostController::~DrivePickerHostController() = default;

void DrivePickerHostController::ShowDrivePickerHost(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  // Ensure that we only have one Drive Picker Host view at a time.
  if (widget_) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        std::move(result_handler))
        ->OnError(drive_picker_host::mojom::DrivePickerError::kAlreadyActive);
    return;
  }

  ui::BaseWindow* window = browser_window_interface_->GetWindow();
  if (!window) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        std::move(result_handler))
        ->OnError(drive_picker_host::mojom::DrivePickerError::kWindowNotFound);
    return;
  }

  auto view = std::make_unique<DrivePickerHostView>(
      browser_window_interface_->GetProfile());
  // Ensure the view has a non-zero preferred size before creating the widget.
  // Mac does not support zero-sized windows, and this also ensures the dialog
  // starts with the correct dimensions (covering the entire browser window).
  view->SetPreferredSize(window->GetBounds().size());

  gfx::NativeWindow native_window = window->GetNativeWindow();
  if (!native_window) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        std::move(result_handler))
        ->OnError(drive_picker_host::mojom::DrivePickerError::kWindowNotFound);
    return;
  }

  delegate_ = std::make_unique<views::DialogDelegate>();
  delegate_->set_use_custom_frame(true);
  delegate_->SetCanResize(false);
  // Ensure the widget is owned by the controller (via unique_ptr).
  delegate_->SetOwnershipOfNewWidget(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET);

  // The Drive Picker Host is designed to be a window-modal overlay. This means
  // that while the picker UI or the initial consent dialog is visible, it will
  // block all user interactions with the underlying browser window, including:
  // 1. The web content area of all tabs in the window.
  // 2. The browser's chrome (e.g., the tab strip, the omnibox, and the main
  //    menu).
  //
  // This modality is crucial for several reasons:
  // - Security/Anti-Spoofing: By covering the entire window and blocking
  //   interaction, we prevent the user from accidentally (or maliciously)
  //   interacting with the page below while they are making a
  //   security-sensitive decision (like granting access to their Google Drive).
  // - Focus Management: It ensures the user's attention remains on the picker
  //   flow until it is completed or explicitly cancelled.
  // - Lifecycle Integrity: Blocking interaction with the rest of the window
  //   prevents edge cases where the browser window's state might change
  //   (e.g., navigating away in a tab) while the picker is still active.
  delegate_->SetModalType(ui::mojom::ModalType::kWindow);
  delegate_->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate_->SetShowTitle(false);
  delegate_->SetShowCloseButton(false);
  DrivePickerHostView* view_ptr = delegate_->SetContentsView(std::move(view));

  // Use standard Chrome constrained window utility to create and show the modal
  // picker overlay. This utility manages the lifecycle and positioning of the
  // dialog relative to the browser window, ensuring it correctly handles
  // window resizes and modality. Despite CreateBrowserModalDialogViews being
  // deprecated, it is the only option for creating a resizable
  // *browser-modal* dialog that supports a WebContents view.
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      delegate_.get(), native_window);

  widget_.reset(widget);

  widget_->SetBounds(window->GetBounds());

  // Ensure the hosted WebContents is transparent. This allows the WebUI to
  // render its own semi-transparent scrim or floating dialog while the
  // browser window remains partially visible underneath.
  if (view_ptr->GetWebContents()) {
    view_ptr->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  }

  // The following settings ensure that the widget's views (root view,
  // non-client view, frame view, and client view) are transparent. This is
  // necessary for the browser scrim to be transparent and correctly show the
  // WebUI host for the Drive Picker. By setting the layers to non-opaque and
  // removing backgrounds/borders, we allow the underlying browser contents to
  // be visible through the semi-transparent scrim.
  views::View* root_view = widget_->GetRootView();
  if (root_view) {
    root_view->SetPaintToLayer();
    root_view->layer()->SetFillsBoundsOpaquely(false);
    root_view->SetBackground(nullptr);
    root_view->SetBorder(nullptr);
  }
  views::NonClientView* non_client_view = widget_->non_client_view();
  if (non_client_view) {
    non_client_view->SetPaintToLayer();
    non_client_view->layer()->SetFillsBoundsOpaquely(false);
    non_client_view->SetBackground(nullptr);
    non_client_view->SetBorder(nullptr);
    if (non_client_view->frame_view()) {
      non_client_view->frame_view()->SetPaintToLayer();
      non_client_view->frame_view()->layer()->SetFillsBoundsOpaquely(false);
      non_client_view->frame_view()->SetBackground(nullptr);

      // BubbleFrameView maintains an internal raw pointer (bubble_border_) to
      // its Border object. Calling SetBorder(nullptr) would leave this pointer
      // dangling, causing crashes during layout.
      //
      // To achieve a transparent frame safely, we provide a "flat"
      // BubbleBorder with zero insets and a transparent color. We then
      // immediately remove the background that SetBubbleBorder automatically
      // adds, as BubbleBackground fills the bubble with a solid color.
      if (auto* bubble_frame_view = views::AsViewClass<views::BubbleFrameView>(
              non_client_view->frame_view())) {
        auto border = std::make_unique<views::BubbleBorder>(
            views::BubbleBorder::FLOAT, views::BubbleBorder::NO_SHADOW);
        border->set_insets(gfx::Insets());
        border->SetColor(SK_ColorTRANSPARENT);
        bubble_frame_view->SetBubbleBorder(std::move(border));
        bubble_frame_view->SetBackground(nullptr);
      } else {
        non_client_view->frame_view()->SetBorder(nullptr);
      }
    }
    if (non_client_view->client_view()) {
      non_client_view->client_view()->SetPaintToLayer();
      non_client_view->client_view()->layer()->SetFillsBoundsOpaquely(false);
      non_client_view->client_view()->SetBackground(nullptr);
      non_client_view->client_view()->SetBorder(nullptr);
    }
  }

  // Ensure the window appears above most other browser-level UI elements
  // (like the omnibox dropdown or status bubble) by using a floating Z-order.
  widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);

  widget_->MakeCloseSynchronous(
      base::BindOnce(&DrivePickerHostController::ResetControllerState,
                     weak_ptr_factory_.GetWeakPtr()));

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

  widget_->Activate();
  widget_->Show();
}

void DrivePickerHostController::ResetControllerState(
    views::Widget::ClosedReason reason) {
  widget_.reset();
  delegate_.reset();
  is_picker_document_loaded_ = false;
  pending_picker_result_handler_.reset();
  Observe(nullptr);
}

void DrivePickerHostController::DocumentOnLoadCompletedInPrimaryMainFrame() {
  is_picker_document_loaded_ = true;

  // We use `pending_picker_result_handler_` to check if there was a request to
  // trigger the picker UI before the document finished loading. This controller
  // only manages a single active picker session at a time.
  if (pending_picker_result_handler_ && widget_ &&
      widget_->widget_delegate()->GetContentsView()) {
    views::AsViewClass<DrivePickerHostView>(
        widget_->widget_delegate()->GetContentsView())
        ->TriggerDrivePickerHostUi(std::move(pending_picker_result_handler_));
  }
}
