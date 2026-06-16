// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

class Profile;
class BrowserWindowInterface;
namespace content {
class WebContents;
}
namespace input {
struct NativeWebKeyboardEvent;
}

// `DrivePickerHostView` provides the UI container and management for the Google
// Drive Picker overlay. It hosts a `views::WebView` that loads the
// chrome://drive-picker-host `WebUI`.
//
// Architecture & Security:
// This view acts as the trusted "bridge" in Chrome's security model for the
// Drive Picker. The hosted chrome:// `WebUI` privileged frame contains an
// iframe pointing to chrome-untrusted://drive-picker-host, which isolates
// the third-party Drive Picker API from privileged browser bindings.
//
// UI Presentation:
// It is designed to be hosted as the contents view of a browser-modal dialog,
// centering itself dynamically within the browser window boundaries.
//
// Ownership and Lifetime:
// This object is owned by the `views::View` hierarchy and its lifetime is
// managed by the `views::Widget` that hosts it. It is created by
// `DrivePickerHostController`, which tracks its existence to relay results
// back to the AI Mode/Compose components.
class DrivePickerHostView : public views::View,
                            public content::WebContentsDelegate {
  METADATA_HEADER(DrivePickerHostView, views::View)

 public:
  explicit DrivePickerHostView(
      Profile* profile,
      BrowserWindowInterface* browser_window_interface);
  DrivePickerHostView(const DrivePickerHostView&) = delete;
  DrivePickerHostView& operator=(const DrivePickerHostView&) = delete;
  ~DrivePickerHostView() override;

  // Returns the `WebContents` hosted by this view's `WebView`.
  content::WebContents* GetWebContents();

  // Calls into the `WebUI` to trigger the Drive Picker UI and relays results to
  // the provided result handler.
  void TriggerDrivePickerHostUi(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request);

  // `views::View`:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void RequestFocus() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // `content::WebContentsDelegate`:
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  // `HandleKeyboardEvent()` is called when the `WebContents` (the `WebView`)
  // receives a keyboard event that was not handled by the page's JavaScript or
  // consumed by the renderer.
  //
  // These unhandled keyboard events are bubbled back to the Views
  // `FocusManager` (using `unhandled_keyboard_event_handler_`) so that standard
  // browser-level keyboard shortcuts (like Cmd+T, Cmd+W) and tab focus
  // traversal continue to work.
  //
  // Escape Key Handling:
  // The Escape key is handled at two different levels to guarantee
  // responsiveness:
  // 1. When focus is inside the `WebContents` (specifically inside the
  //    cross-origin Google Drive Picker iframe, which traps keyboard inputs),
  //    the Escape key is intercepted at the very beginning of the pipeline by
  //    `PreHandleKeyboardEvent()`. Since it returns `HANDLED` there, the key
  //    event is consumed early and will never bubble down here.
  // 2. When focus is outside the `WebContents` (e.g. on the dialog delegate
  //    borders), the event is handled via the `views::FocusManager`
  //    accelerator registered in the constructor (`AcceleratorPressed()`).
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostViewTest, Initialization);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostViewTest, TriggerDrivePickerHostUi);

  // Reports an error back to the result handler in the request.
  void SendErrorToRequest(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request,
      drive_picker_host::mojom::DrivePickerError error);

  views::ViewTracker view_tracker_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_
