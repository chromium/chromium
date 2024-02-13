// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/osauth/apply_online_password_screen_handler.h"

namespace ash {

ApplyOnlinePasswordScreenHandler::ApplyOnlinePasswordScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ApplyOnlinePasswordScreenHandler::~ApplyOnlinePasswordScreenHandler() = default;

void ApplyOnlinePasswordScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<ApplyOnlinePasswordScreenView>
ApplyOnlinePasswordScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
