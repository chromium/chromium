// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_save_item_data.h"
#include "components/download/public/common/download_item.h"

namespace download {
namespace {

// This memory address is used as unique key by SupportsUserData.
const void* const kSaveItemData = &kSaveItemData;

}  // namespace

DownloadSaveItemData::DownloadSaveItemData(std::vector<ItemInfo>&& item_infos)
    : item_infos_(std::move(item_infos)) {}

DownloadSaveItemData::~DownloadSaveItemData() = default;

// static
void DownloadSaveItemData::AttachItemData(DownloadItem* download_item,
                                          std::vector<ItemInfo> item_infos) {
  download_item->SetUserData(
      kSaveItemData,
      std::make_unique<DownloadSaveItemData>(std::move(item_infos)));
}

// static
std::vector<DownloadSaveItemData::ItemInfo>* DownloadSaveItemData::GetItemData(
    DownloadItem* download_item) {
  auto* item_data = static_cast<DownloadSaveItemData*>(
      download_item->GetUserData(kSaveItemData));
  return !item_data ? nullptr : &item_data->item_infos_;
}

}  // namespace download
