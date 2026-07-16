// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

namespace omnibox_everywhere {

OmniboxEverywhereController::OmniboxEverywhereController()
    : ui_manager_(std::make_unique<OmniboxEverywhereUIManager>()) {}

OmniboxEverywhereController::~OmniboxEverywhereController() = default;

void OmniboxEverywhereController::OnInvoke(InvocationSource source,
                                           Profile* profile) {
  switch (source) {
    case InvocationSource::kGlobalHotkey:
      if (IsVisible() && ui_manager_->profile() == profile) {
        Close();
      } else {
        ui_manager_->ShowForProfile(profile);
      }
      break;
  }
}

void OmniboxEverywhereController::Close() {
  ui_manager_->Close();
}

bool OmniboxEverywhereController::IsVisible() const {
  return ui_manager_->IsVisible();
}

void OmniboxEverywhereController::ShutdownForProfile(Profile* profile) {
  if (profile == ui_manager_->profile()) {
    ui_manager_->Shutdown();
  }
}

}  // namespace omnibox_everywhere
