// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_IDS_H_

#include "ui/base/identifier/unique_identifier.h"

DECLARE_UNIQUE_IDENTIFIER_TYPE(SidePanelAnimationId);

#define DECLARE_SIDE_PANEL_ANIMATION_ID(Name) \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(SidePanelAnimationId, Name)

// Maps to the side panel bounds animation when it is opened or closed.
DECLARE_SIDE_PANEL_ANIMATION_ID(kSidePanelBoundsAnimation);

// Maps to the shadow elevations opacity value when the side panel is opened.
DECLARE_SIDE_PANEL_ANIMATION_ID(kShadowOverlayOpacityAnimation);

// Maps to the side panel content's top bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentTopBoundAnimation);

// Maps to the side panel content's bottom bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentBottomBoundAnimation);

// Maps to the side panel content's left bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentLeftBoundAnimation);

// Maps to the side panel content's width bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_SIDE_PANEL_ANIMATION_ID(kSidePanelContentWidthBoundAnimation);

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_IDS_H_
