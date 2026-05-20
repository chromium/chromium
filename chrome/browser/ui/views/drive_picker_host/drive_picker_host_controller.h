// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

class DrivePickerHostView;
class BrowserWindowInterface;

// Window-level orchestrator for the Drive Picker Host, responsible for
// managing the creation, display, and lifetime of the overlay that hosts
// either the user consent dialog and/or the Google Drive Picker UI. It is only
// responsible for a single active picker session at a time.
//
// UI Presentation & Architecture:
// To ensure the overlay precisely covers the entire browser window (including
// the tab strip, toolbar, and web contents) without "spilling over" beyond the
// browser's visible edges, this controller hosts the DrivePickerHostView
// directly as a child of the BrowserView.
//
// By staying within the BrowserView's view hierarchy, we leverage the Views
// framework's built-in clipping, which is strictly enforced against the
// window's client area across all platforms. This avoids the complexities and
// platform-specific inconsistencies (e.g., OS-level window shadows or borders)
// inherent in using a separate top-level TYPE_POPUP widget.
//
// Ownership and Lifetime:
// This class is owned by ContextualSearchboxHandler and follows its
// lifetime. It is instantiated to manage the UI flow triggered when a user
// selects "Upload from Drive".
class DrivePickerHostController : public content::WebContentsObserver,
                                  public views::WidgetObserver {
 public:
  explicit DrivePickerHostController(
      BrowserWindowInterface* browser_window_interface);
  DrivePickerHostController(const DrivePickerHostController&) = delete;
  DrivePickerHostController& operator=(const DrivePickerHostController&) =
      delete;
  ~DrivePickerHostController() override;

  // Shows the Drive Picker Host (either a consent dialog or the picker
  // UI), and relays results to the provided result handler in the request.
  virtual void ShowDrivePickerHost(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request);

 private:
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostControllerTest,
                           ShowDrivePickerHostCreatesView);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostControllerTest,
                           ResetControllerStateClearsView);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostControllerTest,
                           PickerCoversBrowserContents);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostControllerTest,
                           PickerResizesWithWindow);

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Reports an error back to the result handler in the request.
  void SendErrorToRequest(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request,
      drive_picker_host::mojom::DrivePickerError error);

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  // Resets the controller's state, destroying the overlay view and clearing
  // observations and pending handlers.
  void ResetControllerState();

  // Updates the bounds of the picker view to match the browser window.
  void UpdatePickerViewBounds();

  // Whether the Drive Picker document has completed loading in the WebView.
  bool is_picker_document_loaded_ = false;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  views::ViewTracker view_tracker_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_window_observation_{this};

  // Stores the request if the picker document is not yet loaded when
  // ShowDrivePickerHost is called.
  std::unique_ptr<drive_picker_host::DrivePickerHostRequest> pending_request_;

  base::WeakPtrFactory<DrivePickerHostController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_
