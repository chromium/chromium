// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fjord_fw_update_screen_handler.h"

#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

FjordFwUpdateScreenHandler::FjordFwUpdateScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FjordFwUpdateScreenHandler::~FjordFwUpdateScreenHandler() = default;

void FjordFwUpdateScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<FjordFwUpdateScreenView> FjordFwUpdateScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FjordFwUpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("fjordFwUpdateTitle", IDS_FJORD_FW_UPDATE_TITLE);
  builder->Add("fjordFwUpdateSubtitle", IDS_FJORD_FW_UPDATE_SUBTITLE);
}

}  // namespace ash
