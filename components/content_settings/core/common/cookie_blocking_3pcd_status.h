// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_BLOCKING_3PCD_STATUS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_BLOCKING_3PCD_STATUS_H_

// Enum to denote what the current 3PC blocking status is for 3PCD.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class CookieBlocking3pcdStatus {
  // The user is not in 3PCD so 3PCD blocking status does not apply.
  kNotIn3pcd = 0,
  // The user is in 3PCD with 3PC limited (i.e. blocked with mitigations).
  kLimited = 1,
  // The user is in 3PCD with all 3PC blocked.
  kAll = 2,
  kMaxValue = kAll,
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_BLOCKING_3PCD_STATUS_H_
