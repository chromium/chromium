// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/views/widget/widget.h"

namespace views {
class Widget;
class DialogDelegate;
}  // namespace views

class DrivePickerHostView;
class BrowserWindowInterface;

// Window-level orchestrator for the Drive Picker Host, responsible for
// managing the creation, display, and lifetime of the overlay that hosts
// either the user consent dialog and/or the Google Drive Picker UI. It is only
// responsible for a single active picker session at a time.
//
// Ownership and Lifetime:
// This class is owned by ContextualSearchboxHandler and follows its
// lifetime. It is instantiated to manage the UI flow triggered when a user
// selects "Upload from Drive". When the user selects "Upload from Drive", the
// DrivePickerHostController::ShowDrivePickerHost is called, which creates and
// shows the DrivePickerHostView, which hosts a WebUI that will be
// responsible for rendering the consent dialog and/or the Google Drive Picker
// UI and relaying the results to the provided result handler.
//
// Scope and Concurrency:
// As a window-level orchestrator, it renders the picker UI over the entire
// browser window's contents to prevent spoofing and ensure visibility.
// It is designed to manage a single active picker session at a time;
// the controller handles re-entrancy by ensuring only one instance of the
// picker UI (consent or file selection) is active for the associated window.
class DrivePickerHostController : public content::WebContentsObserver {
 public:
  explicit DrivePickerHostController(
      BrowserWindowInterface* browser_window_interface);
  DrivePickerHostController(const DrivePickerHostController&) = delete;
  DrivePickerHostController& operator=(const DrivePickerHostController&) =
      delete;
  ~DrivePickerHostController() override;

  // Shows the Drive Picker Host (either a consent dialog or the picker
  // UI), and relays results to the provided result handler.
  virtual void ShowDrivePickerHost(
      mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
          result_handler);

 private:
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostControllerTest,
                           ShowDrivePickerHostCreatesView);
  FRIEND_TEST_ALL_PREFIXES(DrivePickerHostControllerTest,
                           WidgetCloseResetsState);

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Resets the controller's state, destroying the widget and clearing
  // observations and pending handlers.
  void ResetControllerState(views::Widget::ClosedReason reason);

  // Whether the Drive Picker document has completed loading in the WebView.
  bool is_picker_document_loaded_ = false;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<views::Widget> widget_;

  // Stores the result handler if the picker document is not yet loaded when
  // ShowDrivePickerHost is called.
  mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
      pending_picker_result_handler_;

  base::WeakPtrFactory<DrivePickerHostController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_
