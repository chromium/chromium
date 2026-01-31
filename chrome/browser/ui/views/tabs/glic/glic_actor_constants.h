// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_GLIC_ACTOR_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_GLIC_ACTOR_CONSTANTS_H_

namespace glic {

// Rounded and flat edge radii used to style the GlicButton and
// GlicActorTaskIcon as a split button (two buttons that operate together).
// The two buttons share a flat edge in the middle, with rounded edges on their
// respective left and right sides.
constexpr int kSplitButtonFlatEdgeRadius = 2;
constexpr int kSplitButtonRoundedEdgeRadius = 10;

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_GLIC_ACTOR_CONSTANTS_H_
