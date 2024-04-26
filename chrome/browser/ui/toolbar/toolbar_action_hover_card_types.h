// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_TYPES_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_TYPES_H_

// TODO(crbug.com/40857356): Mergue with
// `TabSlotController::HoverCardUpdateType` once the base hover card controller
// class once it's implemented.
enum class ToolbarActionHoverCardUpdateType {
  kHover,
  kEvent,
  kToolbarActionUpdated,
  kToolbarActionRemoved
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_TYPES_H_
