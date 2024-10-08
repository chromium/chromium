// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/batch_upload_handler.h"

#include <algorithm>
#include <iterator>

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/ui/webui/signin/batch_upload/batch_upload.mojom.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Construct the `BatchUploadData` structure to be used in the Ui. Combining the
// account info, dialog subtitle and a list of data containers.
// The Data contaiers are a list items to be shown on the batch upload ui.
// Sending the information using the Mojo equivalent structures:
// `BatchUploadDataContainer` -> `batch_upload::mojom::DataContainer`
// `BatchUploadDataItem` -> `batch_upload::mojom::DataItem`
batch_upload::mojom::BatchUploadDataPtr ConstructMojoBatchUploadData(
    const AccountInfo& account_info,
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list) {
  CHECK(!account_info.IsEmpty());
  CHECK(!data_providers_list.empty());

  batch_upload::mojom::BatchUploadAccountInfoPtr account_info_mojo =
      batch_upload::mojom::BatchUploadAccountInfo::New();
  account_info_mojo->email = account_info.email;
  account_info_mojo->data_picture_url =
      signin::GetAccountPictureUrl(account_info);

  std::vector<batch_upload::mojom::DataContainerPtr> data_containers_mojo;
  for (const auto& data_provider : data_providers_list) {
    BatchUploadDataContainer container = data_provider->GetLocalData();
    CHECK(!container.items.empty());

    batch_upload::mojom::DataContainerPtr data_container_mojo =
        batch_upload::mojom::DataContainer::New();
    data_container_mojo->section_title =
        l10n_util::GetStringUTF8(container.section_title_id);

    for (const auto& data_item : container.items) {
      batch_upload::mojom::DataItemPtr data_item_mojo =
          batch_upload::mojom::DataItem::New();
      data_item_mojo->id = data_item.id.value();
      data_item_mojo->icon_url = data_item.icon_url.spec();
      data_item_mojo->title = data_item.title;
      data_item_mojo->subtitle = data_item.subtitle;

      data_container_mojo->data_items.push_back(std::move(data_item_mojo));
    }
    data_containers_mojo.push_back(std::move(data_container_mojo));
  }

  batch_upload::mojom::BatchUploadDataPtr batch_upload_mojo =
      batch_upload::mojom::BatchUploadData::New();

  batch_upload_mojo->account_info = std::move(account_info_mojo);
  // TODO(b/365954465): This string is still not comlpete and should depend on
  // the first `container` input.
  batch_upload_mojo->dialog_subtitle =
      l10n_util::GetStringUTF8(IDS_BATCH_UPLOAD_SUBTITLE);
  batch_upload_mojo->data_containers = std::move(data_containers_mojo);

  return batch_upload_mojo;
}

}  // namespace

BatchUploadHandler::BatchUploadHandler(
    mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver,
    mojo::PendingRemote<batch_upload::mojom::Page> page,
    const AccountInfo& account_info,
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list,
    base::RepeatingCallback<void(int)> update_view_height_callback,
    SelectedDataTypeItemsCallback completion_callback)
    : data_providers_list_(data_providers_list),
      update_view_height_callback_(update_view_height_callback),
      completion_callback_(std::move(completion_callback)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  page_->SendBatchUploadData(
      ConstructMojoBatchUploadData(account_info, data_providers_list));
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

void BatchUploadHandler::SaveToAccount(
    const std::vector<std::vector<int32_t>>& ids_to_move) {
  CHECK_EQ(ids_to_move.size(), data_providers_list_.size());

  // Convert `ids_to_move` ids from `int32_t` to `BatchUploadDataItemModel::Id`
  // with a map outer container instead of a vector. The order of the vector
  // matches with the order of `data_providers_list_`.
  base::flat_map<BatchUploadDataType, std::vector<BatchUploadDataItemModel::Id>>
      ret_ids_to_move;
  for (size_t i = 0; i < ids_to_move.size(); ++i) {
    std::vector<BatchUploadDataItemModel::Id> section_ids;
    std::ranges::transform(
        ids_to_move[i], std::back_inserter(section_ids),
        [](int32_t id) { return BatchUploadDataItemModel::Id(id); });
    ret_ids_to_move.insert_or_assign(data_providers_list_[i]->GetDataType(),
                                     section_ids);
  }

  data_providers_list_.clear();
  std::move(completion_callback_).Run(ret_ids_to_move);
}
