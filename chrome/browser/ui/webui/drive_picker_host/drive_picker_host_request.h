// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_REQUEST_H_
#define CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_REQUEST_H_

#include <optional>

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace drive_picker_host {

// Encapsulates a request to show the Drive Picker Host, specifying which
// UI components to display and providing handles for results.
//
// Usage Expectations:
// - `result_handler` must be provided to receive results or errors from the
//   picker or disclaimer dialog.
class DrivePickerHostRequest {
 public:
  enum class RequestType { kConsentDialog, kPickerUi };

  DrivePickerHostRequest(
      RequestType type,
      mojo::PendingRemote<mojom::DrivePickerResultHandler> result_handler);

  // DrivePickerHostRequest is move-only.
  DrivePickerHostRequest(const DrivePickerHostRequest&) = delete;
  DrivePickerHostRequest& operator=(const DrivePickerHostRequest&) = delete;
  DrivePickerHostRequest(DrivePickerHostRequest&&);
  DrivePickerHostRequest& operator=(DrivePickerHostRequest&&);
  ~DrivePickerHostRequest();

  RequestType type() const { return type_; }

  bool has_result_handler() const { return !!result_handler_; }

  mojo::PendingRemote<mojom::DrivePickerResultHandler> TakeResultHandler() {
    return std::move(result_handler_);
  }

 private:
  // The type of request (e.g., show consent dialog or picker UI).
  RequestType type_;

  // Handle to receive the results of the Drive Picker or disclaimer dialog
  // (selected files, cancellation, or errors).
  mojo::PendingRemote<mojom::DrivePickerResultHandler> result_handler_;
};

}  // namespace drive_picker_host

#endif  // CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_REQUEST_H_
