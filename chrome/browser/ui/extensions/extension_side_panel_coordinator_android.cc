// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_coordinator.h"

namespace extensions {

// static
std::unique_ptr<ExtensionSidePanelCoordinator::Delegate>
ExtensionSidePanelCoordinator::CreateDelegate(
    ExtensionSidePanelCoordinator* coordinator) {
  return nullptr;
}

}  // namespace extensions
