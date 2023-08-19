// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_CONSENT_STATE_H_
#define COMPONENTS_UKM_UKM_CONSENT_STATE_H_

#include "base/containers/enum_set.h"

namespace ukm {

// Different types of consents that control what types of data can be recorded
// by UKM.
enum UkmConsentType {
  // "Make searches and browsing better" (MSBB) is consented.
  // MSBB is toggled on in chrome://settings for all user profiles.
  MSBB,
  // Extensions are consented, depends on MSBB.
  // Separately controls recording of chrome-extension:// URLs.
  // This flag should reflect the "Extensions" user setting
  // found in chrome://settings/syncSetup/advanced.
  EXTENSIONS,
  // App Sync is consented, depends on MSBB.
  // Controls recording of app:// URLs.
  // This flag should reflect the "Apps" user setting
  // found in chrome://settings/syncSetup/advanced.
  APPS,
};

// Collection of UKM consent type states that have been granted.
using UkmConsentState =
    base::EnumSet<UkmConsentType, UkmConsentType::MSBB, UkmConsentType::APPS>;
}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_CONSENT_STATE_H_