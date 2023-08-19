// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATUS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATUS_H_

// Status of the Cookie controls controller, used by CookieControlsIconView
// and OldCookieControlsBubbleView.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: CookieControlsStatus
enum class CookieControlsStatus {
  kUninitialized,
  // Third-Party cookie blocking is enabled.
  kEnabled,
  // Third-Party cookie blocking is disabled.
  kDisabled,
  // Third-Party cookie blocking is enabled in general but was disabled
  // for this site.
  kDisabledForSite,
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_CONTROLS_STATUS_H_
