// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"

namespace ash {

HoldingSpaceClientImpl::HoldingSpaceClientImpl(Profile* profile)
    : profile_(profile) {}

HoldingSpaceClientImpl::~HoldingSpaceClientImpl() = default;

void HoldingSpaceClientImpl::OpenItem(const HoldingSpaceItem& item,
                                      OpenItemCallback callback) {
  if (item.file_path().empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  file_manager::util::OpenItem(
      profile_, item.file_path(), platform_util::OPEN_FILE,
      base::BindOnce(
          [](OpenItemCallback callback,
             platform_util::OpenOperationResult result) {
            const bool success = result == platform_util::OPEN_SUCCEEDED;
            std::move(callback).Run(success);
          },
          std::move(callback)));
}

}  // namespace ash
