// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_page_handler.h"

#include "base/files/file_path.h"

namespace chromeos::cloud_upload {

CloudUploadPageHandler::CloudUploadPageHandler(
    mojo::PendingReceiver<chromeos::cloud_upload::mojom::PageHandler>
        pending_page_handler)
    : receiver_{this, std::move(pending_page_handler)} {}

CloudUploadPageHandler::~CloudUploadPageHandler() = default;

void CloudUploadPageHandler::GetUploadPath(GetUploadPathCallback callback) {
  std::move(callback).Run(std::move(base::FilePath("/from Chromebook")));
}

}  // namespace chromeos::cloud_upload
