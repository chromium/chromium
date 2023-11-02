// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONSTANTS_H_
#define COMPONENTS_PERMISSIONS_CONSTANTS_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace permissions {

// The URL for the Bluetooth Overview help center article in the Web Bluetooth
// Chooser.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kChooserBluetoothOverviewURL[];

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPostPromptSurveyPromptDispositionKey[];

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPostPromptSurveyHadGestureKey[];

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPostPromptSurveyPromptDispositionReasonKey[];
#endif

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONSTANTS_H_
