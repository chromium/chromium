// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_TRACKING_PROTECTION_FEATURE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_TRACKING_PROTECTION_FEATURE_H_

#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"

namespace content_settings {

// Enum to denote the type of an ACT feature.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class TrackingProtectionFeatureType {
  kUnknownFeature = 0,
  kThirdPartyCookies = 1,
  kFingerprintingProtection = 2,
  kIpProtection = 3,
  kMaxValue = kIpProtection,
};

// Enum to denote blocking status for ACT features.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class TrackingProtectionBlockingStatus {
  kUnknownState = 0,
  kAllowed = 1,  // 3PCs, digital fingerprinting
  kLimited = 2,  // 3PCs, digital fingerprinting
  kBlocked = 3,  // 3PCs
  kHidden = 4,   // IP address
  kVisible = 5,  // IP address
  kMaxValue = kVisible,
};

// Struct that contains all information needed to render an ACT feature state
// within in-context surfaces (user bypass, page info).
struct TrackingProtectionFeature {
  bool operator==(TrackingProtectionFeature const&) const = default;
  TrackingProtectionFeatureType feature_type;
  CookieControlsEnforcement enforcement;
  TrackingProtectionBlockingStatus status;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_TRACKING_PROTECTION_FEATURE_H_
