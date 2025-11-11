// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATE_H_

// Enum representing the state of in-context cookie controls.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class CookieControlsState {
  // Controls not visible.
  kHidden = 0,
  // Third-party cookies UI with toggle off.
  kBlocked3pc = 1,
  // Third-party cookies UI with toggle on.
  kAllowed3pc = 2,
  kMaxValue = kAllowed3pc,
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATE_H_
