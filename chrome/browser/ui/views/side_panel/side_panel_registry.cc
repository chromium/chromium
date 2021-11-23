// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

SidePanelRegistry::SidePanelRegistry() = default;

SidePanelRegistry::~SidePanelRegistry() = default;

void SidePanelRegistry::Register(std::unique_ptr<SidePanelEntry> entry) {
  entries_.push_back(std::move(entry));
}
