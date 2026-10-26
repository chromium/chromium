// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/webview/web_contents_delegate_views.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "url/gurl.h"

class Profile;
class BrowserWindowInterface;
namespace content {
class WebContents;
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
class DrivePickerHostView
    : public views::View,
      public views::WebContentsDelegateViews<DrivePickerHostView>,
      public DrivePickerHostUI::Delegate {
  METADATA_HEADER(DrivePickerHostView, views::View)

 public:
  explicit DrivePickerHostView(
      Profile* profile,
      BrowserWindowInterface* browser_window_interface,
      drive_picker_host::DrivePickerHostRequest::RequestType initial_ui_type);
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
  void AddedToWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // DrivePickerHostUI::Delegate:
  void OnTransitionToPicker() override;
  void OnTransitionToError() override;
  void OnTransitionToConsent() override;

  // `content::WebContentsDelegate`:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostViewTest, Initialization);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostViewTest, TriggerDrivePickerHostUi);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostViewTest,
                           SetsCornerRadiusOnAddedToWidget);

  // Reports an error back to the result handler in the request.
  void SendErrorToRequest(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request,
      drive_picker_host::mojom::DrivePickerError error);

  views::ViewTracker view_tracker_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  drive_picker_host::DrivePickerHostRequest::RequestType current_ui_type_ =
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_VIEW_H_
