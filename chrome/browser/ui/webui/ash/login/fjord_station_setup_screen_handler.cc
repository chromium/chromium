// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fjord_station_setup_screen_handler.h"

namespace ash {

FjordStationSetupScreenHandler::FjordStationSetupScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FjordStationSetupScreenHandler::~FjordStationSetupScreenHandler() = default;

void FjordStationSetupScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<FjordStationSetupScreenView>
FjordStationSetupScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
