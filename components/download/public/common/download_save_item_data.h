// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_ITEM_DATA_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_ITEM_DATA_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/supports_user_data.h"
#include "components/download/public/common/download_item.h"
#include "url/gurl.h"

namespace download {

// Additional information for `DownloadItem` about all files in a save_package.
// It is only available when finishing the download.
class COMPONENTS_DOWNLOAD_EXPORT DownloadSaveItemData
    : public base::SupportsUserData::Data {
 public:
  struct ItemInfo {
    // The final path where this file of the package will be saved.
    base::FilePath file_path;
    // The url this file was downloaded from.
    GURL url;
    // The referrer url for this file. (In case of a package download this is
    // the main page for the other resources.)
    GURL referrer_url;
  };

  explicit DownloadSaveItemData(std::vector<ItemInfo>&& item_infos);
  ~DownloadSaveItemData() override;

  // Add the information about all save item of this download item.
  static void AttachItemData(DownloadItem* download_item,
                             std::vector<ItemInfo> item_infos);

  // Get the information about all save items of the download item.
  static std::vector<ItemInfo>* GetItemData(DownloadItem* download_item);

 private:
  std::vector<ItemInfo> item_infos_;
};

}  // namespace download

#endif  //  COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_ITEM_DATA_H_
