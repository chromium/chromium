// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"

#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace projects_panel {

std::unique_ptr<views::HighlightPathGenerator>
GetListItemHighlightPathGenerator() {
  return std::make_unique<views::RoundRectHighlightPathGenerator>(
      /*insets=*/gfx::Insets(0),
      /*corner_radius=*/kListItemCornerRadius);
}

}  // namespace projects_panel
