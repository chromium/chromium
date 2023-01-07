// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_UTILS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_UTILS_H_

#include "chromeos/crosapi/mojom/download_controller.mojom-forward.h"
#include "components/download/public/common/download_export.h"

namespace download {

class DownloadItem;

namespace download_item_utils {

// Returns a new `crosapi::mojom::DownloadItemPtr` from the specified
// `DownloadItem*`.
COMPONENTS_DOWNLOAD_EXPORT
crosapi::mojom::DownloadItemPtr ConvertToMojoDownloadItem(
    const DownloadItem* download_item,
    bool is_from_incognito_profile);

}  // namespace download_item_utils
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_UTILS_H_
