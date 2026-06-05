// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

class Profile;
class BrowserWindowInterface;
namespace content {
class WebContents;
}
namespace views {
class WebView;
}

// DrivePickerHostView provides the UI container and management for the Google
// Drive Picker overlay. It hosts a views::WebView that loads the
// chrome://drive-picker-host WebUI.
//
// Architecture & Security:
// This view acts as the trusted "bridge" in Chrome's security model for the
// Drive Picker. The hosted chrome:// WebUI privileged frame contains an
// iframe pointing to chrome-untrusted://drive-picker-host, which isolates
// the third-party Drive Picker API from privileged browser bindings.
//
// UI Presentation:
// It is designed to be rendered as a child view of BrowserView, covering the
// entire browser window area (including top chrome and web contents). This
// ensures strict clipping to the browser window bounds and consistent
// cross-platform behavior by avoiding OS-level window decorations or alignment
// issues associated with separate top-level widgets.
//
// Ownership and Lifetime:
// This object is owned by the views::View hierarchy and its lifetime is
// managed by the views::Widget that hosts it. It is created by
// DrivePickerHostController, which tracks its existence to relay results
// back to the AI Mode/Compose components.
class DrivePickerHostView : public views::View {
  METADATA_HEADER(DrivePickerHostView, views::View)

 public:
  explicit DrivePickerHostView(
      Profile* profile,
      BrowserWindowInterface* browser_window_interface);
  DrivePickerHostView(const DrivePickerHostView&) = delete;
  DrivePickerHostView& operator=(const DrivePickerHostView&) = delete;
  ~DrivePickerHostView() override;

  // Returns the WebContents hosted by this view's WebView.
  content::WebContents* GetWebContents();

  // Calls into the WebUI to trigger the Drive Picker UI and relays results to
  // the provided result handler.
  void TriggerDrivePickerHostUi(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void RequestFocus() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostViewTest, Initialization);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostViewTest, TriggerDrivePickerHostUi);

  // Reports an error back to the result handler in the request.
  void SendErrorToRequest(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request,
      drive_picker_host::mojom::DrivePickerError error);

  views::ViewTracker view_tracker_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_
