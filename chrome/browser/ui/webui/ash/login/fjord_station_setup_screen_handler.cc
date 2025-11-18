// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fjord_station_setup_screen_handler.h"

#include "chrome/grit/generated_resources.h"

namespace ash {

FjordStationSetupScreenHandler::FjordStationSetupScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FjordStationSetupScreenHandler::~FjordStationSetupScreenHandler() = default;

void FjordStationSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("fjordStationSetupNextButton",
               IDS_FJORD_STATION_SETUP_NEXT_BUTTON_TEXT);
  builder->Add("fjordStationSetupDoneButton",
               IDS_FJORD_STATION_SETUP_DONE_BUTTON_TEXT);
}

void FjordStationSetupScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<FjordStationSetupScreenView>
FjordStationSetupScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
