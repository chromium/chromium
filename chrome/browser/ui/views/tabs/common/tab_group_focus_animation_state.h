// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_FOCUS_ANIMATION_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_FOCUS_ANIMATION_STATE_H_

#include <optional>

#include "components/tab_groups/tab_group_color.h"

// Appearance of a tab group's header and Unfocus button for one frame of the
// focus motion. A default-constructed state restores the resting appearance.
struct TabGroupFocusAnimationState {
  // 0.0 (unfocused appearance) to 1.0 (focused appearance). `nullopt` uses the
  // resting appearance for the group's current focused state.
  std::optional<float> progress;

  // Alpha applied to the header label and icons and to the Unfocus label.
  float content_opacity = 1.0f;

  // Horizontal shift of the content while switching between focused groups.
  int content_horizontal_offset = 0;

  // While switching between focused groups, the background is blended toward
  // `blend_color_id` by `color_blend`.
  std::optional<tab_groups::TabGroupColorId> blend_color_id;
  float color_blend = 0.0f;

  friend bool operator==(const TabGroupFocusAnimationState&,
                         const TabGroupFocusAnimationState&) = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_FOCUS_ANIMATION_STATE_H_
