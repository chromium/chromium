// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

DrivePickerHostView::DrivePickerHostView(
    Profile* profile,
    BrowserWindowInterface* browser_window_interface,
    drive_picker_host::DrivePickerHostRequest::RequestType initial_ui_type)
    : browser_window_interface_(browser_window_interface),
      current_ui_type_(initial_ui_type) {
  // Set the view to paint to a layer so that the view can be transparent over
  // the web contents. This allows the web contents to appear like a floating
  // dialog over the browser window.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBackground(nullptr);
  SetBorder(nullptr);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::WebView* web_view =
      AddChildView(std::make_unique<views::WebView>(profile));
  view_tracker_.SetView(web_view);
  web_view->SetBackground(nullptr);

  // Since the `WebView` was just created with a valid profile,
  // `GetWebContents()` is guaranteed to return a valid pointer. We use a
  // `CHECK` here to assert this state and remove redundant defensive checks.
  CHECK(web_view->GetWebContents());
  web_view->GetWebContents()->SetDelegate(this);
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  web_view->LoadInitialURL(GURL(chrome::kChromeUIDrivePickerHostURL));

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

DrivePickerHostView::~DrivePickerHostView() {
  content::WebContents* contents = GetWebContents();
  if (contents && contents->GetWebUI()) {
    auto* drive_picker_host_ui =
        contents->GetWebUI()->GetController()->GetAs<DrivePickerHostUI>();
    if (drive_picker_host_ui) {
      drive_picker_host_ui->set_delegate(nullptr);
    }
  }
}

content::WebContents* DrivePickerHostView::GetWebContents() {
  if (!view_tracker_.view()) {
    return nullptr;
  }
  return views::AsViewClass<views::WebView>(view_tracker_.view())
      ->GetWebContents();
}

gfx::Size DrivePickerHostView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size;
  if (current_ui_type_ ==
      drive_picker_host::DrivePickerHostRequest::RequestType::kConsentDialog) {
    // Tight fit for the Google ConsentKit card
    size = gfx::Size(520, 580);
  } else if (current_ui_type_ == drive_picker_host::DrivePickerHostRequest::
                                     RequestType::kErrorDialog) {
    // Small size for the error dialog
    size = gfx::Size(512, 180);
  } else {
    // Standard size for the Google Drive Picker UI
    size = gfx::Size(830, 600);
  }
  return size;
}

void DrivePickerHostView::OnTransitionToPicker() {
  current_ui_type_ =
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi;
  PreferredSizeChanged();
  if (GetWidget()) {
    GetWidget()->CenterWindow(GetPreferredSize());
  }
}

void DrivePickerHostView::OnTransitionToError() {
  current_ui_type_ =
      drive_picker_host::DrivePickerHostRequest::RequestType::kErrorDialog;
  PreferredSizeChanged();
  if (GetWidget()) {
    GetWidget()->CenterWindow(GetPreferredSize());
  }
}

void DrivePickerHostView::RequestFocus() {
  views::WebView* web_view =
      views::AsViewClass<views::WebView>(view_tracker_.view());
  if (web_view) {
    web_view->RequestFocus();
    if (web_view->GetWebContents()) {
      web_view->GetWebContents()->Focus();
    }
  }
}

void DrivePickerHostView::AddedToWidget() {
  views::View::AddedToWidget();
  views::WebView* web_view =
      views::AsViewClass<views::WebView>(view_tracker_.view());
  if (web_view) {
    // Remove rounded corners to align with the Drive Picker's rectangular look.
    web_view->holder()->SetNativeViewCornerRadii(gfx::RoundedCornersF(0));

    // Also ensure this view's layer is rectangular.
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(0));
  }
}

bool DrivePickerHostView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    views::Widget* widget = GetWidget();
    if (widget) {
      widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
    }
    return true;
  }
  return false;
}

void DrivePickerHostView::TriggerDrivePickerHostUi(
    std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request) {
  current_ui_type_ = request->type();
  PreferredSizeChanged();
  if (GetWidget()) {
    GetWidget()->CenterWindow(GetPreferredSize());
  }

  if (!view_tracker_.view()) {
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        handler = request->TakeResultHandler();
    if (handler) {
      SendErrorToRequest(
          std::move(request),
          drive_picker_host::mojom::DrivePickerError::kViewNotFound);
    }
    return;
  }
  views::WebView* web_view =
      views::AsViewClass<views::WebView>(view_tracker_.view());
  if (!web_view) {
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        handler = request->TakeResultHandler();
    if (handler) {
      SendErrorToRequest(
          std::move(request),
          drive_picker_host::mojom::DrivePickerError::kViewNotFound);
    }
    return;
  }
  content::WebContents* contents = web_view->GetWebContents();
  if (contents && contents->GetWebUI()) {
    auto* drive_picker_host_ui =
        contents->GetWebUI()->GetController()->GetAs<DrivePickerHostUI>();
    if (drive_picker_host_ui) {
      drive_picker_host_ui->set_delegate(this);
      drive_picker_host_ui->TriggerDrivePickerHost(std::move(request));
      return;
    }
  }

  mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
      handler = request->TakeResultHandler();
  if (handler) {
    SendErrorToRequest(
        std::move(request),
        drive_picker_host::mojom::DrivePickerError::kWebUINotFound);
  }
}

void DrivePickerHostView::SendErrorToRequest(
    std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request,
    drive_picker_host::mojom::DrivePickerError error) {
  if (request && request->has_result_handler()) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        request->TakeResultHandler())
        ->OnError(error);
  }
}

content::WebContents* DrivePickerHostView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (!browser_window_interface_) {
    return nullptr;
  }
  if (!params.url.SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }
  content::OpenURLParams new_params(params);
  if (new_params.disposition == WindowOpenDisposition::CURRENT_TAB ||
      new_params.disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    new_params.disposition = WindowOpenDisposition::NEW_WINDOW;
  }
  return browser_window_interface_->OpenURL(
      new_params, std::move(navigation_handle_callback));
}

content::WebContents* DrivePickerHostView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (!browser_window_interface_) {
    if (was_blocked) {
      *was_blocked = true;
    }
    return nullptr;
  }
  if (!target_url.SchemeIsHTTPOrHTTPS()) {
    if (was_blocked) {
      *was_blocked = true;
    }
    return nullptr;
  }
  WindowOpenDisposition new_disposition = disposition;
  if (new_disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      new_disposition == WindowOpenDisposition::CURRENT_TAB) {
    new_disposition = WindowOpenDisposition::NEW_WINDOW;
  }
  chrome::AddWebContents(browser_window_interface_, source,
                         std::move(new_contents), target_url, new_disposition,
                         window_features);
  return nullptr;
}

content::KeyboardEventProcessingResult
DrivePickerHostView::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == ui::VKEY_ESCAPE &&
      event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    views::Widget* widget = GetWidget();
    if (widget) {
      widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
    }
    return content::KeyboardEventProcessingResult::HANDLED;
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}
bool DrivePickerHostView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (!event.os_event) {
    return false;
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

BEGIN_METADATA(DrivePickerHostView)
END_METADATA
