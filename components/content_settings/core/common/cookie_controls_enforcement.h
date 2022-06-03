// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_ENFORCEMENT_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_ENFORCEMENT_H_

// Enum to denote whether cookie controls are enforced, and how.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class CookieControlsEnforcement {
  kNoEnforcement = 0,
  kEnforcedByPolicy = 1,
  kEnforcedByExtension = 2,
  kEnforcedByCookieSetting = 3,
  kMaxValue = kEnforcedByCookieSetting,
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_ENFORCEMENT_H_
