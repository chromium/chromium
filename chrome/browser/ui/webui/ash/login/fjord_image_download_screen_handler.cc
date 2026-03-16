// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fjord_image_download_screen_handler.h"

#include "chrome/grit/generated_resources.h"

namespace ash {

FjordImageDownloadScreenHandler::FjordImageDownloadScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FjordImageDownloadScreenHandler::~FjordImageDownloadScreenHandler() = default;

void FjordImageDownloadScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("fjordImageDownloadTitle", IDS_FJORD_IMAGE_DOWNLOAD_TITLE);
}

void FjordImageDownloadScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<FjordImageDownloadScreenView>
FjordImageDownloadScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
