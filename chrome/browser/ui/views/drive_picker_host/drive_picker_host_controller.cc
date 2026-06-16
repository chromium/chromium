// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

// `DrivePickerDialogDelegate` configures a window-modal dialog containing the
// `WebContents` view. It suppresses standard Chrome dialog decorations (title,
// buttons, margins) since the `WebContents` renders its own internal controls,
// and enforces client widget ownership for asynchronous lifetime management.
class DrivePickerDialogDelegate : public views::DialogDelegate {
 public:
  explicit DrivePickerDialogDelegate(
      std::unique_ptr<views::View> contents_view) {
    SetModalType(ui::mojom::ModalType::kWindow);
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    SetShowCloseButton(false);
    SetShowTitle(false);
    set_margins(gfx::Insets());
    SetContentsView(std::move(contents_view));
    SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  }
};

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

  // We host the view inside a standard browser-modal dialog widget using the
  // `constrained_window` framework to prevent interaction with the parent
  // window (such as the omnibox or tabs) while the picker is active.
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
  picker_view_ = view.get();

  auto delegate = std::make_unique<DrivePickerDialogDelegate>(std::move(view));
  delegate->RegisterWindowClosingCallback(
      base::BindOnce(&DrivePickerHostController::ResetControllerState,
                     weak_ptr_factory_.GetWeakPtr()));

  picker_delegate_ = std::move(delegate);

  picker_widget_ =
      base::WrapUnique(constrained_window::CreateBrowserModalDialogViews(
          picker_delegate_.get(),
          browser_view->GetWidget()->GetNativeWindow()));

  if (auto* frame_view = picker_widget_->widget_delegate()
                             ->AsDialogDelegate()
                             ->GetBubbleFrameView()) {
    frame_view->SetBackgroundColor(SK_ColorTRANSPARENT);
  }

  picker_widget_->Show();
  picker_widget_observation_.Observe(picker_widget_.get());

  // Explicitly request focus on the newly shown picker view to immediately
  // capture keyboard focus and prevent browser shortcuts from leaking.
  view_ptr->RequestFocus();

  // Ensure the hosted `WebContents` is transparent. This allows the `WebUI` to
  // render its own semi-transparent scrim or floating dialog while the
  // browser window remains partially visible underneath.
  if (view_ptr->GetWebContents()) {
    view_ptr->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  }

  // The controller observes the `WebUI`'s `WebContents` to be notified when the
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<views::Widget> widget,
                          std::unique_ptr<views::DialogDelegate> delegate) {
                         widget.reset();
                         delegate.reset();
                       },
                       std::move(picker_widget_), std::move(picker_delegate_)));
  } else {
    picker_delegate_.reset();
  }
  picker_view_ = nullptr;
  is_picker_document_loaded_ = false;
  pending_request_.reset();
  Observe(nullptr);
  if (on_close_callback_) {
    std::move(on_close_callback_).Run();
  }
}

void DrivePickerHostController::OnWidgetDestroying(views::Widget* widget) {
  if (widget == picker_widget_.get()) {
    ResetControllerState();
  }
}

void DrivePickerHostController::DocumentOnLoadCompletedInPrimaryMainFrame() {
  is_picker_document_loaded_ = true;

  // We use `pending_request_` to check if there was a request to
  // trigger the picker UI before the document finished loading. This controller
  // only manages a single active picker session at a time.
  if (pending_request_ && picker_view_) {
    picker_view_->TriggerDrivePickerHostUi(std::move(pending_request_));
  }
}
