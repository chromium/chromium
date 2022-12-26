// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/constants.h"

namespace permissions {

const char kChooserBluetoothOverviewURL[] =
    "https://support.google.com/chrome?p=bluetooth";

#if !BUILDFLAG(IS_ANDROID)
// The key in `Product Specific String Data` under which the disposition of the
// permission prompt is recorded in the post-prompt HaTS survey.
const char kPermissionsPostPromptSurveyPromptDispositionKey[] =
    "PromptDisposition";

// The key in `Product Specific String Data` under which the quiet UI reason is
// recorded in the post-prompt HaTS survey.
const char kPermissionsPostPromptSurveyPromptDispositionReasonKey[] =
    "PromptDispositionReason";

// The key in `Product Specific String Data` under which the request action is
// recorded in the post-prompt HaTS survey.
const char kPermissionsPostPromptSurveyActionKey[] = "Action";

// The key in `Product Specific String Data` under which the request type is
// recorded in the post-prompt HaTS survey.
const char kPermissionsPostPromptSurveyRequestTypeKey[] = "RequestType";

// The key in `Product Specific Bits Data` under which whether the prompt was
// triggered by a user gestured is recorded in the post-prompt HaTS survey.
const char kPermissionsPostPromptSurveyHadGestureKey[] = "HadGesture";

// The key in `Product Specific Bits Data` under which the release channel on
// which the prompt was triggered is recorded in the post-prompt HaTS survey.
// Note that a finch config typically defines a min_version to run the
// experiment. When Version V is stable, Beta (V+1), Dev (V+2) and Canary (V+3)
// all have higher version numbers and will therefore be part of the experiment
// with min_version V with the rollout plan for stable. This filter allows
// restriction to specific channels (typically to stable).
const char kPermissionsPostPromptSurveyReleaseChannelKey[] = "ReleaseChannel";
#endif

}  // namespace permissions
