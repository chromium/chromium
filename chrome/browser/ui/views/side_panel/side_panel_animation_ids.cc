// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"

#include "ui/base/identifier/unique_identifier.h"

#define DEFINE_SIDE_PANEL_ANIMATION_ID(Name) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(SidePanelAnimationId, Name)

DEFINE_SIDE_PANEL_ANIMATION_ID(kSidePanelBoundsAnimation);

DEFINE_SIDE_PANEL_ANIMATION_ID(kShadowOverlayOpacityAnimation);

DEFINE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentTopBoundAnimation);
DEFINE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentBottomBoundAnimation);
DEFINE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentLeftBoundAnimation);
DEFINE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentWidthBoundAnimation);
