// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/frame_view.h"

// `DrivePickerDialogDelegate` configures a window-modal dialog containing the
// `WebContents` view. It suppresses standard Chrome dialog decorations (title,
// buttons, margins) since the `WebContents` renders its own internal controls,
// and enforces client widget ownership for asynchronous lifetime management.
class DrivePickerDialogDelegate : public views::DialogDelegate {
 public:
  // `use_desktop_widget` requests a top-level desktop widget on Desktop Aura
  // platforms (Windows and Linux), where `ChromeViewsDelegate` would otherwise
  // create a `NativeWidgetAura` child clipped to a smaller non-browser anchor
  // window (e.g. the detached Glic floaty). On macOS (`NativeWidgetMac`) and
  // ChromeOS Ash, dialogs are already unclipped top-level `NSWindow`s or
  // display-root children, so this override is a no-op there.
  explicit DrivePickerDialogDelegate(std::unique_ptr<views::View> contents_view,
                                     bool use_desktop_widget = false) {
    SetModalType(ui::mojom::ModalType::kWindow);
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    SetShowCloseButton(false);
    SetShowTitle(false);
    set_margins(gfx::Insets());
    SetContentsView(std::move(contents_view));
    SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    set_use_desktop_widget_override(use_desktop_widget);
  }

  std::unique_ptr<views::FrameView> CreateFrameView(
      views::Widget* widget) override {
    auto frame =
        std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::NO_SHADOW);
    border->set_rounded_corners(gfx::RoundedCornersF(0));
    frame->SetBubbleBorder(std::move(border));
    frame->SetBackgroundColor(SK_ColorTRANSPARENT);
    return frame;
  }
};

DrivePickerHostController::DrivePickerHostController(
    Profile* profile,
    BrowserWindowInterface* browser_window_interface,
    views::Widget* anchor_widget)
    : profile_(profile),
      browser_window_interface_(browser_window_interface),
      anchor_widget_(anchor_widget ? anchor_widget->GetWeakPtr() : nullptr),
      is_standalone_popup_(anchor_widget != nullptr) {}

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
  web_modal::WebContentsModalDialogHost* dialog_host = nullptr;
  if (browser_window_interface_) {
    dialog_host =
        browser_window_interface_->GetWebContentsModalDialogHostForWindow();
  }

  const bool is_omnibox_everywhere =
      base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere);
  const bool is_glic_request =
      request->style() ==
      drive_picker_host::DrivePickerHostRequest::Style::kGlic;
  const bool is_glic = is_glic_request &&
                       base::FeatureList::IsEnabled(features::kGlicDrivePicker);

  // Loomnibox (`kOmniboxEverywhere`) anchors directly to its standalone popup
  // `anchor_widget_` when present (or opens unparented if no browser window
  // exists). Glic always passes its panel/floaty widget as `anchor_widget_`, so
  // it prefers the active browser window's `dialog_host` and only falls back to
  // `anchor_widget_` when no browser window is open (`!dialog_host`). Glic
  // requests never use Loomnibox anchoring, whatever the state of
  // `kOmniboxEverywhere`.
  const bool use_loomnibox_anchoring = !is_glic_request &&
                                       is_standalone_popup_ &&
                                       is_omnibox_everywhere && anchor_widget_;
  const bool use_glic_floaty_anchoring =
      !dialog_host && is_glic && anchor_widget_;
  const bool use_anchor_widget =
      use_loomnibox_anchoring || use_glic_floaty_anchoring;

  const bool allows_no_dialog_host =
      is_glic_request ? use_glic_floaty_anchoring : is_omnibox_everywhere;
  if (!dialog_host && !allows_no_dialog_host) {
    SendErrorToRequest(
        std::move(request),
        drive_picker_host::mojom::DrivePickerError::kWindowNotFound);
    return;
  }

  auto view = std::make_unique<DrivePickerHostView>(
      profile_, browser_window_interface_, request->type(),
      is_glic ? DrivePickerHostView::Style::kGlic
              : DrivePickerHostView::Style::kDefault);
  DrivePickerHostView* view_ptr = view.get();
  picker_view_ = view.get();

  // A Glic picker anchored to the floaty must not be clipped to the floaty's
  // (much smaller) window on Desktop Aura platforms, so it is created as a
  // top-level desktop widget.
  const bool use_desktop_widget = use_glic_floaty_anchoring;
  auto delegate = std::make_unique<DrivePickerDialogDelegate>(
      std::move(view), use_desktop_widget);
  delegate->RegisterWindowClosingCallback(
      base::BindOnce(&DrivePickerHostController::ResetControllerState,
                     weak_ptr_factory_.GetWeakPtr()));

  picker_delegate_ = std::move(delegate);

  views::Widget* widget = nullptr;
  gfx::NativeView parent_view = gfx::NativeView();
  views::Widget* parent_widget = nullptr;
  if (use_anchor_widget) {
    parent_view = anchor_widget_->GetNativeView();
    parent_widget = anchor_widget_.get();
  } else if (dialog_host) {
    parent_view = dialog_host->GetHostView();
    parent_widget = views::Widget::GetWidgetForNativeView(parent_view);
  }

  widget = views::DialogDelegate::CreateDialogWidget(
      picker_delegate_.get(), gfx::NativeWindow(), parent_view);
  widget->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(
          constrained_window::kConstrainedWindowWidgetIdentifier));

  picker_widget_ = base::WrapUnique(widget);

  if (parent_widget) {
    browser_widget_observation_.Observe(parent_widget);
  }

  picker_widget_->Show();
  DrivePickerHostView::CenterPickerWidget(picker_widget_.get(),
                                          picker_view_->GetPreferredSize());
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
  browser_widget_observation_.Reset();
  anchor_widget_ = nullptr;
  browser_window_interface_ = nullptr;
  Observe(nullptr);
  if (picker_view_) {
    picker_view_->RemoveAllChildViews();
    picker_view_ = nullptr;
  }
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
  is_picker_document_loaded_ = false;
  pending_request_.reset();
  if (on_close_callback_) {
    std::move(on_close_callback_).Run();
  }
}

void DrivePickerHostController::OnWidgetDestroying(views::Widget* widget) {
  ResetControllerState();
}

void DrivePickerHostController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (widget != picker_widget_.get() && picker_widget_) {
    DrivePickerHostView::CenterPickerWidget(
        picker_widget_.get(),
        picker_widget_->GetContentsView()->GetPreferredSize({}));
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
