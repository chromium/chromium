// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_IDS_H_

#include "ui/base/interaction/element_identifier.h"

// Maps to the side panel bounds animation when it is opened or closed.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelBoundsAnimation);

// Maps to the shadow elevations opacity value when the side panel is opened.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kShadowOverlayOpacityAnimation);

// Maps to the side panel content's top bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentTopBoundAnimation);

// Maps to the side panel content's bottom bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentBottomBoundAnimation);

// Maps to the side panel content's left bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentLeftBoundAnimation);

// Maps to the side panel content's width bound animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentWidthBoundAnimation);

// Maps to the side panel content's opacity animation when it is opened and
// transitioned from the provided starting bounds.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentOpacityAnimation);

// Maps to the side panel content's corner radius animation when it is opened
// and transitioned from the provided starting bounds.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentCornerRadiusAnimation);

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_IDS_H_
