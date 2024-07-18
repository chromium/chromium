// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/constants.h"
#include "base/time/clock.h"

namespace permissions {

const char kChooserBluetoothOverviewURL[] =
    "https://support.google.com/chrome?p=bluetooth";

const char kEmbeddedContentHelpCenterURL[] =
    "https://support.google.com/chrome/?p=embedded_content";

// The key in `Product Specific String Data` under which the disposition of the
// permission prompt is recorded in the prompt HaTS survey.
const char kPermissionsPromptSurveyPromptDispositionKey[] = "PromptDisposition";

// The key in `Product Specific String Data` under which the quiet UI reason is
// recorded in the prompt HaTS survey.
const char kPermissionsPromptSurveyPromptDispositionReasonKey[] =
    "PromptDispositionReason";

// The key in `Product Specific String Data` under which the request action is
// recorded in the prompt HaTS survey.
const char kPermissionsPromptSurveyActionKey[] = "Action";

// The key in `Product Specific String Data` under which the request type is
// recorded in the prompt HaTS survey.
const char kPermissionsPromptSurveyRequestTypeKey[] = "RequestType";

// The key in `Product Specific String Data` under which the display timing of
// the survey is recorded in the prompt HaTS survey.
extern const char kPermissionsPromptSurveyDisplayTimeKey[] =
    "SurveyDisplayTime";

// The key in `Product Specific String Data` under which the 'one time prompts
// decided' count bucket of the user taking the prompt HaTS survey is recorded.
extern const char kPermissionPromptSurveyOneTimePromptsDecidedBucketKey[] =
    "OneTimePromptsDecidedBucket";

// The key in `Product Specific String Data` under which the URL on which the
// prompt HaTS survey was triggered is recorded.
extern const char kPermissionPromptSurveyUrlKey[] = "PromptSurveyUrl";

// The key in `Product Specific Bits Data` under which whether the prompt
// was triggered by a user gestured is recorded in the prompt HaTS survey.
const char kPermissionsPromptSurveyHadGestureKey[] = "HadGesture";

// The key in `Product Specific String Data` under which the release channel on
// which the prompt was triggered is recorded in the prompt HaTS survey.
// Note that a finch config typically defines a min_version to run the
// experiment. When Version V is stable, Beta (V+1), Dev (V+2) and Canary (V+3)
// all have higher version numbers and will therefore be part of the experiment
// with min_version V with the rollout plan for stable. This filter allows
// restriction to specific channels (typically to stable).
const char kPermissionsPromptSurveyReleaseChannelKey[] = "ReleaseChannel";

const char kPermissionPromptSurveyPepcPromptPositionKey[] =
    "PepcPromptPosition";

const char kPermissionPromptSurveyInitialPermissionStatusKey[] =
    "InitialPermissionStatus";

// TODO(crbug.com/40254381): Remove the code related to unused site permissions
// from Android builds.

const char kRevokedKey[] = "revoked";

const char kRevokedChooserPermissionsKey[] = "revoked-chooser-permissions";

const base::TimeDelta kStorageAccessAPIExplicitPermissionLifetime =
    base::Days(30);

const base::TimeDelta kStorageAccessAPIImplicitPermissionLifetime =
    base::Hours(24);

const base::TimeDelta kStorageAccessAPIRelatedWebsiteSetsLifetime =
    base::Days(30);

}  // namespace permissions
