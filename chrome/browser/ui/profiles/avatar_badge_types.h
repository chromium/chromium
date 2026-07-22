// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_AVATAR_BADGE_TYPES_H_
#define CHROME_BROWSER_UI_PROFILES_AVATAR_BADGE_TYPES_H_

// Operation types for SVG wave path rendering of the profile avatar badge.
enum class AvatarBadgePathOp {
  kMoveTo = 0,
  kLineTo = 1,
  kCubicTo = 2,
  kClose = 3,
};

#endif  // CHROME_BROWSER_UI_PROFILES_AVATAR_BADGE_TYPES_H_
