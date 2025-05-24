// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"

#include "base/functional/callback.h"

BatchUploadUIDelegate::BatchUploadUIDelegate() = default;

BatchUploadUIDelegate::~BatchUploadUIDelegate() = default;

void BatchUploadUIDelegate::ShowBatchUploadDialog(
    Browser* browser,
    std::vector<syncer::LocalDataDescription> local_data_description_list,
    BatchUploadService::EntryPoint entry_point,
    BatchUploadSelectedDataTypeItemsCallback complete_callback) {
  CHECK(browser);
  ShowBatchUploadDialogInternal(*browser,
                                std::move(local_data_description_list),
                                entry_point, std::move(complete_callback));
}
