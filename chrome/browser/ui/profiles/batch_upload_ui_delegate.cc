// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"

#include "base/functional/callback.h"

BatchUploadUIDelegate::BatchUploadUIDelegate() = default;

BatchUploadUIDelegate::~BatchUploadUIDelegate() = default;

void BatchUploadUIDelegate::ShowBatchUploadDialog(
    Browser* browser,
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list,
    SelectedDataTypeItemsCallback complete_callback) {
  CHECK(browser);
  ShowBatchUploadDialogInternal(*browser, data_providers_list,
                                std::move(complete_callback));
}
