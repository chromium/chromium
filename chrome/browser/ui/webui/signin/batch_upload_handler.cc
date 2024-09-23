// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/batch_upload_handler.h"

#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"

BatchUploadHandler::BatchUploadHandler(
    mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver,
    mojo::PendingRemote<batch_upload::mojom::Page> page,
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list,
    base::RepeatingCallback<void(int)> update_view_height_callback,
    SelectedDataTypeItemsCallback completion_callback)
    : data_providers_list_(data_providers_list),
      update_view_height_callback_(update_view_height_callback),
      completion_callback_(std::move(completion_callback)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  // Temporary code; should expand to show all the data in
  // `data_providers_list_`.
  CHECK(!data_providers_list_.empty());
  BatchUploadDataContainer container = data_providers_list_[0]->GetLocalData();
  CHECK(!container.items.empty());
  page_->SendData(container.items[0].title);
}

BatchUploadHandler::~BatchUploadHandler() = default;

void BatchUploadHandler::UpdateViewHeight(uint32_t height) {
  update_view_height_callback_.Run(height);
}

void BatchUploadHandler::Close() {
  // Clear the data as after `completion_callback_` is done, the data owners
  // will be destroyed.
  data_providers_list_.clear();
  std::move(completion_callback_).Run({});
}
