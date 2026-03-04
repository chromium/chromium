// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon.h"

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace glic {

ToolbarGlicActorTaskIcon::ToolbarGlicActorTaskIcon(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback pressed_callback)
    : GlicActorTaskIcon<ToolbarButton>(browser_window_interface,
                                       pressed_callback) {}

ToolbarGlicActorTaskIcon::~ToolbarGlicActorTaskIcon() = default;

void ToolbarGlicActorTaskIcon::UpdateColors() {
  ToolbarButton::UpdateColorsAndInsets();
}

BEGIN_METADATA(ToolbarGlicActorTaskIcon)
END_METADATA
}  // namespace glic
