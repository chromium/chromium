// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_page_handler.h"

#include "base/files/file_path.h"

namespace ash::cloud_upload {

CloudUploadPageHandler::CloudUploadPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    RespondAndCloseCallback callback)
    : receiver_{this, std::move(pending_page_handler)},
      callback_{std::move(callback)} {}

CloudUploadPageHandler::~CloudUploadPageHandler() = default;

void CloudUploadPageHandler::RespondAndClose(mojom::UserAction action) {
  if (callback_) {
    std::move(callback_).Run(action);
  }
}

}  // namespace ash::cloud_upload
