// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HOVERCARD_HOVER_CARD_UPDATE_TYPE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HOVERCARD_HOVER_CARD_UPDATE_TYPE_H_

// Describes the reason a hover card show, hide, or update was requested.
enum class HoverCardUpdateType {
  kHover,
  kFocus,
  kTabDataChanged,
  kAnimating,
  kTabRemoved,
  kSelectionChanged,
  kEvent,
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HOVERCARD_HOVER_CARD_UPDATE_TYPE_H_
