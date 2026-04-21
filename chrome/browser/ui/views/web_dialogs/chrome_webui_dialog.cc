// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_dialogs/chrome_webui_dialog.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace webui_dialog {

WebDialogSpec::WebDialogSpec() = default;
WebDialogSpec::~WebDialogSpec() = default;
WebDialogSpec::WebDialogSpec(const WebDialogSpec&) = default;

// static
std::unique_ptr<views::Widget> ChromeWebUIDialog::Show(
    gfx::NativeWindow parent,
    std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
    const WebDialogSpec& spec) {
  auto dialog =
      std::make_unique<ChromeWebUIDialog>(std::move(contents_wrapper), spec);
  ChromeWebUIDialog* dialog_ptr = dialog.get();

  views::Widget* widget = nullptr;

  if (spec.modal_type == ui::mojom::ModalType::kChild) {
    CHECK(spec.parent_tab)
        << "kChild (tab-modal) dialogs require spec.parent_tab";
    widget = constrained_window::CreateWebModalDialogViews(
        dialog.release(), spec.parent_tab->GetContents());
  } else if (spec.modal_type == ui::mojom::ModalType::kWindow ||
             spec.modal_type == ui::mojom::ModalType::kSystem) {
    widget = constrained_window::CreateBrowserModalDialogViews(dialog.release(),
                                                               parent);
  } else {
    // Allows for non-modal unanchored dialog options.
    widget = views::DialogDelegate::CreateDialogWidget(
        dialog.release(), /*context=*/parent,
        /*parent=*/gfx::NativeView());
  }

  // Observe the widget so the delegate can safely clean itself up.
  dialog_ptr->widget_observation_.Observe(widget);

  if (!spec.wait_for_explicit_show) {
    widget->Show();
  }

  return base::WrapUnique(widget);
}

ChromeWebUIDialog::ChromeWebUIDialog(
    std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
    const WebDialogSpec& spec)
    : spec_(spec), contents_wrapper_(std::move(contents_wrapper)) {
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  SetButtons(spec_.buttons);
  SetModalType(spec_.modal_type);
  SetShowCloseButton(spec_.show_close_button);
  SetHasWindowSizeControls(true);

  set_margins(gfx::Insets());

  auto web_view = std::make_unique<views::WebView>(
      contents_wrapper_->web_contents()->GetBrowserContext());
  web_view_ = web_view.get();
  web_view_->SetWebContents(contents_wrapper_->web_contents());

  if (spec_.element_identifier) {
    web_view_->SetProperty(views::kElementIdentifierKey,
                           spec_.element_identifier);
  }

  // Establish an initial preferred size. Mac does not support zero-sized
  // windows, and other platforms may hit DCHECKs during Widget::Init if bounds
  // are empty.
  gfx::Size initial_size = spec_.min_size;
  if (initial_size.IsEmpty()) {
    initial_size = gfx::Size(1, 1);
  }
  web_view_->SetPreferredSize(initial_size);
  web_view_->EnableSizingFromWebContents(spec_.min_size, spec_.max_size);

  view_observation_.Observe(web_view_);
  SetContentsView(std::move(web_view));

  contents_wrapper_->SetHost(weak_ptr_factory_.GetWeakPtr());
}

ChromeWebUIDialog::~ChromeWebUIDialog() = default;

views::View* ChromeWebUIDialog::GetInitiallyFocusedView() {
  return web_view_;
}

void ChromeWebUIDialog::ShowUI() {
  if (!GetWidget()) {
    return;
  }

  // Note: On some platforms (such as Mac or Wayland), the widget's visibility
  // state is updated asynchronously.
  if (!GetWidget()->IsVisible()) {
    GetWidget()->Show();
  }
}

void ChromeWebUIDialog::CloseUI() {
  if (GetWidget()) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void ChromeWebUIDialog::ResizeDueToAutoResize(content::WebContents* source,
                                              const gfx::Size& new_size) {
  if (!GetWidget()) {
    return;
  }

  gfx::Size bounded_size = new_size;

  // The `new_size` comes from the renderer, which is instructed to respect
  // the min/max bounds via `EnableAutoResize()` in the constructor. However,
  // because the renderer is an untrusted process, the size must be defensively
  // clamped here to guarantee strict adherence to `spec_`.
  bounded_size.SetToMax(spec_.min_size);
  bounded_size.SetToMin(spec_.max_size);

  // Ensure the dialog doesn't exceed the display's work area.
  // Note: Clamping against the work area occurs *after* applying the spec
  // bounds in case the spec max_size is larger than the screen.
  gfx::Rect work_area = GetWidget()->GetWorkAreaBoundsInScreen();

  if (!work_area.IsEmpty()) {
    // The `bounded_size` represents the web content size, but the widget must
    // fit within the work area. Calculate the maximum content size by
    // subtracting the frame's size (borders, shadows) from the work area.
    gfx::Size frame_size = GetWidget()
                               ->non_client_view()
                               ->GetWindowBoundsForClientBounds(gfx::Rect())
                               .size();

    const int max_content_width =
        std::max(0, work_area.width() - frame_size.width());
    const int max_content_height =
        std::max(0, work_area.height() - frame_size.height());
    if (bounded_size.height() > max_content_height) {
      bounded_size.set_height(max_content_height);
    }
    if (bounded_size.width() > max_content_width) {
      bounded_size.set_width(max_content_width);
    }
  }

  web_view_->SetPreferredSize(bounded_size);

  // Resize the widget to fit the new preferred size of the contents.
  // The non-client view includes the window frame, so this ensures the
  // entire dialog is sized correctly.
  GetWidget()->CenterWindow(GetWidget()->non_client_view()->GetPreferredSize());
}

void ChromeWebUIDialog::OnViewAddedToWidget(views::View* observed_view) {
  if (observed_view != web_view_) {
    return;
  }

  // Apply rounded corners to the NativeViewHost to prevent WebUI content
  // from bleeding through the dialog's rounded corners.
  //
  // TODO(https://crbug.com/344626785): Remove this once DialogDelegate
  // natively supports rounded corners.
  web_view_->holder()->SetCornerRadii(
      gfx::RoundedCornersF(spec_.corner_radius.value_or(GetCornerRadius())));
}

void ChromeWebUIDialog::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view == web_view_) {
    view_observation_.Reset();
    web_view_ = nullptr;
  }
}

void ChromeWebUIDialog::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

}  // namespace webui_dialog
