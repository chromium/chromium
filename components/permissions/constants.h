// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONSTANTS_H_
#define COMPONENTS_PERMISSIONS_CONSTANTS_H_

#include "base/component_export.h"
#include "base/time/clock.h"
#include "build/build_config.h"

namespace permissions {

// The URL for the Bluetooth Overview help center article in the Web Bluetooth
// Chooser.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kChooserBluetoothOverviewURL[];

// The URL for the Embedded Content help center article in the SAA permission
// prompt.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kEmbeddedContentHelpCenterURL[];

// The key in `Product Specific String Data` under which the disposition of the
// permission prompt is recorded in the prompt HaTS survey.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPromptSurveyPromptDispositionKey[];

// The key in `Product Specific String Data` under which the quiet UI reason is
// recorded in the prompt HaTS survey.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPromptSurveyPromptDispositionReasonKey[];

// The key in `Product Specific String Data` under which the request action is
// recorded in the prompt HaTS survey.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPromptSurveyActionKey[];

// The key in `Product Specific String Data` under which the request type is
// recorded in the prompt HaTS survey.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPromptSurveyRequestTypeKey[];

// The key in `Product Specific String Data` under which the display timing of
// the survey is recorded in the prompt HaTS survey.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPromptSurveyDisplayTimeKey[];

// The key in `Product Specific String Data` under which the 'one time prompts
// decided' count bucket of the user taking the prompt HaTS survey is recorded.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionPromptSurveyOneTimePromptsDecidedBucketKey[];

// The key in `Product Specific String Data` under which the URL on which the
// prompt HaTS survey was triggered is recorded.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionPromptSurveyUrlKey[];

// The key in `Product Specific Bits Data` under which whether the prompt was
// triggered by a user gestured is recorded in the prompt HaTS survey.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPromptSurveyHadGestureKey[];

// The key in `Product Specific Bits Data` under which the release channel on
// which the prompt was triggered is recorded in the prompt HaTS survey.
// Note that a finch config typically defines a min_version to run the
// experiment. When Version V is stable, Beta (V+1), Dev (V+2) and Canary (V+3)
// all have higher version numbers and will therefore be part of the experiment
// with min_version V with the rollout plan for stable. This filter allows
// restriction to specific channels (typically to stable).
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kPermissionsPromptSurveyReleaseChannelKey[];

// TODO(crbug.com/1410489): Remove the code related to unused site permissions
// from Android builds.

// The key used for marking permissions as revoked, as per the unused site
// permissions module of Safety Check.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kRevokedKey[];

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONSTANTS_H_
