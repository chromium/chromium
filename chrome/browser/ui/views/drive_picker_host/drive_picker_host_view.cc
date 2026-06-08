// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

// DrivePickerWebView is a custom views::WebView subclass used to intercept
// input events before they reach the renderer process.
//
// This is necessary because the Google Drive Picker is loaded inside a
// cross-origin third-party iframe. Under browser same-origin security
// boundaries, keyboard events such as pressing the Escape key are captured and
// trapped inside the iframe, preventing them from bubbling up to the parent
// WebUI page's JavaScript. By subclassing WebView and overriding
// PreHandleKeyboardEvent, we can intercept and handle key events at the browser
// process level before they are sent to the renderer, ensuring the dialog can
// always be closed on Escape regardless of where focus resides.
class DrivePickerWebView : public views::WebView {
  METADATA_HEADER(DrivePickerWebView, views::WebView)

 public:
  explicit DrivePickerWebView(content::BrowserContext* browser_context)
      : views::WebView(browser_context) {}
  ~DrivePickerWebView() override = default;

  // content::WebContentsDelegate:
  // Overrides PreHandleKeyboardEvent to intercept the Escape keypress
  // at the very beginning of the WebContents input pipeline, before it
  // is sent to the renderer. This is critical because the Google Drive
  // Picker runs inside a cross-origin third-party iframe which traps keyboard
  // events and prevents them from bubbling up to our parent WebUI frame.
  // Intercepting it here ensures we can always close and dismiss the hosted
  // widget.
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    if (event.windows_key_code == ui::VKEY_ESCAPE &&
        event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
      views::Widget* widget = GetWidget();
      if (widget) {
        widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      }
      return content::KeyboardEventProcessingResult::HANDLED;
    }
    return views::WebView::PreHandleKeyboardEvent(source, event);
  }
};

BEGIN_METADATA(DrivePickerWebView)
END_METADATA

DrivePickerHostView::DrivePickerHostView(
    Profile* profile,
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {
  // Set the view to paint to a layer so that the view can be transparent over
  // the web contents. This allows the web contents to appear like a floating
  // dialog over the browser window.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBackground(nullptr);
  SetBorder(nullptr);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::WebView* web_view =
      AddChildView(std::make_unique<DrivePickerWebView>(profile));
  view_tracker_.SetView(web_view);
  web_view->SetBackground(nullptr);

  // Since the WebView was just created with a valid profile, GetWebContents()
  // is guaranteed to return a valid pointer. We use a CHECK here to assert
  // this state and remove redundant defensive checks.
  CHECK(web_view->GetWebContents());
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  web_view->LoadInitialURL(GURL(chrome::kChromeUIDrivePickerHostURL));

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

DrivePickerHostView::~DrivePickerHostView() = default;

content::WebContents* DrivePickerHostView::GetWebContents() {
  if (!view_tracker_.view()) {
    return nullptr;
  }
  return views::AsViewClass<views::WebView>(view_tracker_.view())
      ->GetWebContents();
}

gfx::Size DrivePickerHostView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!browser_window_interface_ || !browser_window_interface_->GetWindow()) {
    return gfx::Size();
  }

  return browser_window_interface_->GetWindow()->GetBounds().size();
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

BEGIN_METADATA(DrivePickerHostView)
END_METADATA
