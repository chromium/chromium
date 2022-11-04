// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fake_update_required_screen_handler.h"

namespace ash {

void FakeUpdateRequiredScreenHandler::SetUIState(
    UpdateRequiredView::UIState ui_state) {
  ui_state_ = ui_state;
}

}  // namespace ash
