// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

OmniboxContextMenuController::OmniboxContextMenuController() = default;

OmniboxContextMenuController::~OmniboxContextMenuController() = default;

void OmniboxContextMenuController::ExecuteCommand(int id, int event_flags) {}
