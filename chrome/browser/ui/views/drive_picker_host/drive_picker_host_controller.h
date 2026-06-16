// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_observer.h"

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
// UI Presentation & Architecture:
// The controller hosts the `DrivePickerHostView` inside a standard
// browser-modal dialog widget using the `constrained_window` framework. This
// blocks all mouse and keyboard interaction with the parent browser window,
// intercepts browser window shortcuts (like Cmd+T), and displays the standard
// browser-modal scrim under the dialog.
//
// Ownership and Lifetime:
// This class is owned by `ContextualSearchboxHandler` and follows its
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

  // Sets a callback to be executed asynchronously when the hosted picker widget
  // is closed or destroyed. Callers are expected to use this
  // to cleanly tear down and reset the controller and the Mojo result receiver.
  void set_on_close_callback(base::OnceClosure callback) {
    on_close_callback_ = std::move(callback);
  }

  views::Widget* GetWidgetForTesting() { return picker_widget_.get(); }
  DrivePickerHostView* GetViewForTesting() { return picker_view_; }
  void ResetControllerStateForTesting() { ResetControllerState(); }

 private:

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Reports an error back to the result handler in the request.
  void SendErrorToRequest(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request,
      drive_picker_host::mojom::DrivePickerError error);

  // Resets the controller's state, destroying the overlay view and clearing
  // observations and pending handlers.
  void ResetControllerState();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Whether the Drive Picker document has completed loading in the `WebView`.
  bool is_picker_document_loaded_ = false;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  raw_ptr<DrivePickerHostView> picker_view_ = nullptr;
  std::unique_ptr<views::Widget> picker_widget_;
  std::unique_ptr<views::DialogDelegate> picker_delegate_;

  base::OnceClosure on_close_callback_;

  // Stores the request if the picker document is not yet loaded when
  // `ShowDrivePickerHost` is called.
  std::unique_ptr<drive_picker_host::DrivePickerHostRequest> pending_request_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      picker_widget_observation_{this};

  base::WeakPtrFactory<DrivePickerHostController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_CONTROLLER_H_
