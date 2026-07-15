// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

namespace omnibox_everywhere {

OmniboxEverywhereController::OmniboxEverywhereController()
    : ui_manager_(std::make_unique<OmniboxEverywhereUIManager>()) {}

OmniboxEverywhereController::~OmniboxEverywhereController() = default;

void OmniboxEverywhereController::OnInvoke(InvocationSource source) {
  ui_manager_->Show();
}

}  // namespace omnibox_everywhere
