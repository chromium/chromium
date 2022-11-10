// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_page_handler.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"

namespace ash::cloud_upload {

CloudUploadPageHandler::CloudUploadPageHandler(
    mojom::DialogArgsPtr args,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    RespondAndCloseCallback callback)
    : dialog_args_{std::move(args)},
      receiver_{this, std::move(pending_page_handler)},
      callback_{std::move(callback)} {}

CloudUploadPageHandler::~CloudUploadPageHandler() = default;

void CloudUploadPageHandler::GetDialogArgs(GetDialogArgsCallback callback) {
  std::move(callback).Run(dialog_args_ ? dialog_args_.Clone()
                                       : mojom::DialogArgs::New());
}

void CloudUploadPageHandler::RespondAndClose(mojom::UserAction action) {
  if (callback_) {
    std::move(callback_).Run(action);
  }
}

}  // namespace ash::cloud_upload
