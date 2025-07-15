// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_actor_task_icon.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace glic {

GlicActorTaskIcon::GlicActorTaskIcon(TabStripController* tab_strip_controller,
                                     const gfx::VectorIcon& icon,
                                     const std::u16string& tooltip)
    : TabStripNudgeButton(tab_strip_controller,
                          views::Button::PressedCallback(),
                          views::Button::PressedCallback(),
                          tooltip,
                          kGlicActorTaskIconElementId,
                          Edge::kNone,
                          icon) {
  SetProperty(views::kElementIdentifierKey, kGlicActorTaskIconElementId);
}

GlicActorTaskIcon::~GlicActorTaskIcon() = default;

BEGIN_METADATA(GlicActorTaskIcon)
END_METADATA

}  // namespace glic
