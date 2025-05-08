// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATE_H_

// Enum representing the state of in-context cookie controls.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class CookieControlsState {
  // Controls not visible
  kHidden = 0,
  // Third-party cookies UI with 3PC toggle off
  k3pcsBlocked = 1,
  // Third-party cookies UI with 3PC toggle on
  k3pcsAllowed = 2,
  // Tracking protections UI with tracking protections active button
  kTpActive = 3,
  // Tracking protections UI with tracking protections paused button
  kTpPaused = 4,
  kMaxValue = kTpPaused,
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATE_H_
