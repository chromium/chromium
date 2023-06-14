// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_BREAKAGE_CONFIDENCE_LEVEL_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_BREAKAGE_CONFIDENCE_LEVEL_H_

// The confidence level that a site is broken and the user needs to use cookie
// controls. It takes into account blocked third-party cookie access, exceptions
// lifecycle, site engagement index and recent user activity (like frequent page
// reloads).
enum class CookieControlsBreakageConfidenceLevel {
  kUninitialized,
  kLow,
  kMedium,
  kHigh,
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_BREAKAGE_CONFIDENCE_LEVEL_H_
