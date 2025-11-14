// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fjord_touch_controller_screen_handler.h"

namespace ash {

FjordTouchControllerScreenHandler::FjordTouchControllerScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FjordTouchControllerScreenHandler::~FjordTouchControllerScreenHandler() =
    default;

void FjordTouchControllerScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<FjordTouchControllerScreenView>
FjordTouchControllerScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
