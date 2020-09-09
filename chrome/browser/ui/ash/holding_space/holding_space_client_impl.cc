// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"

namespace ash {

namespace {

using SuccessCallback = HoldingSpaceClient::SuccessCallback;

// Helpers ---------------------------------------------------------------------

// Attempts to open the file or folder at the specified `file_path`.
// Success is returned via the supplied `callback`.
void OpenFileOrFolder(Profile* profile,
                      const base::FilePath& file_path,
                      platform_util::OpenItemType type,
                      SuccessCallback callback) {
  if (file_path.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  file_manager::util::OpenItem(
      profile, file_path, type,
      base::BindOnce(
          [](SuccessCallback callback,
             platform_util::OpenOperationResult result) {
            const bool success = result == platform_util::OPEN_SUCCEEDED;
            std::move(callback).Run(success);
          },
          std::move(callback)));
}

}  // namespace

// HoldingSpaceClientImpl ------------------------------------------------------

HoldingSpaceClientImpl::HoldingSpaceClientImpl(Profile* profile)
    : profile_(profile) {}

HoldingSpaceClientImpl::~HoldingSpaceClientImpl() = default;

// TODO(crbug/1126274): Implement.
void HoldingSpaceClientImpl::CopyToClipboard(const HoldingSpaceItem& item,
                                             SuccessCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*success=*/false);
}

void HoldingSpaceClientImpl::OpenItem(const HoldingSpaceItem& item,
                                      SuccessCallback callback) {
  OpenFileOrFolder(profile_, item.file_path(), platform_util::OPEN_FILE,
                   std::move(callback));
}

void HoldingSpaceClientImpl::OpenItemInFolder(const HoldingSpaceItem& item,
                                              SuccessCallback callback) {
  OpenFileOrFolder(profile_, item.file_path().DirName(),
                   platform_util::OPEN_FOLDER, std::move(callback));
}

// TODO(crbug/1126274): Implement.
void HoldingSpaceClientImpl::PinItem(const HoldingSpaceItem& item) {
  NOTIMPLEMENTED();
}

// TODO(crbug/1126274): Implement.
void HoldingSpaceClientImpl::UnpinItem(const HoldingSpaceItem& item) {
  NOTIMPLEMENTED();
}

}  // namespace ash
