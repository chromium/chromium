// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"

#include "base/functional/callback.h"

BatchUploadUIDelegate::BatchUploadUIDelegate() = default;

BatchUploadUIDelegate::~BatchUploadUIDelegate() = default;

void BatchUploadUIDelegate::ShowBatchUploadDialog(
    Browser* browser,
    std::vector<BatchUploadDataContainer> data_containers_list,
    SelectedDataTypeItemsCallback complete_callback) {
  CHECK(browser);
  ShowBatchUploadDialogInternal(*browser, std::move(data_containers_list),
                                std::move(complete_callback));
}
