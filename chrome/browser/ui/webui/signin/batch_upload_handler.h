// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/ui/webui/signin/batch_upload/batch_upload.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

struct AccountInfo;

// WebUI message handler for the Batch Upload dialog bubble.
class BatchUploadHandler : public batch_upload::mojom::PageHandler {
 public:
  // Initializes the handler with the mojo handlers and the needed information
  // to be displayed as well as callbacks to the main native view.
  BatchUploadHandler(
      mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver,
      mojo::PendingRemote<batch_upload::mojom::Page> page,
      const AccountInfo& account_info,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      base::RepeatingCallback<void(int)> update_view_height_callback,
      SelectedDataTypeItemsCallback completion_callback);
  ~BatchUploadHandler() override;

  BatchUploadHandler(const BatchUploadHandler&) = delete;
  BatchUploadHandler& operator=(const BatchUploadHandler&) = delete;

  // batch_upload::mojom::PageHandler:
  void UpdateViewHeight(uint32_t height) override;
  void SaveToAccount(
      const std::vector<std::vector<int32_t>>& idsToMove) override;
  void Close() override;

 private:
  std::vector<raw_ptr<const BatchUploadDataProvider>> data_providers_list_;
  base::RepeatingCallback<void(int)> update_view_height_callback_;
  SelectedDataTypeItemsCallback completion_callback_;

  // Allows handling received messages from the web ui page.
  mojo::Receiver<batch_upload::mojom::PageHandler> receiver_;
  // Interface to send information to the web ui page.
  mojo::Remote<batch_upload::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_HANDLER_H_
