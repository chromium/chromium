// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/constants.h"
#include "base/time/clock.h"

namespace permissions {

const char kChooserBluetoothOverviewURL[] =
    "https://support.google.com/chrome?p=bluetooth";

#if !BUILDFLAG(IS_ANDROID)
const char kPermissionsPostPromptSurveyPromptDispositionKey[] =
    "PromptDisposition";

const char kPermissionsPostPromptSurveyPromptDispositionReasonKey[] =
    "PromptDispositionReason";

const char kPermissionsPostPromptSurveyActionKey[] = "Action";

const char kPermissionsPostPromptSurveyRequestTypeKey[] = "RequestType";

const char kPermissionsPostPromptSurveyHadGestureKey[] = "HadGesture";

const char kPermissionsPostPromptSurveyReleaseChannelKey[] = "ReleaseChannel";
#endif

// TODO(crbug.com/1410489): Remove the code related to unused site permissions
// from Android builds.

const char kRevokedKey[] = "revoked";

const base::TimeDelta kRevocationCleanUpThreshold = base::Days(30);
}  // namespace permissions
