// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/fake_update_required_screen_handler.h"

namespace chromeos {

void FakeUpdateRequiredScreenHandler::SetUIState(
    UpdateRequiredView::UIState ui_state) {
  ui_state_ = ui_state;
}

}  // namespace chromeos
