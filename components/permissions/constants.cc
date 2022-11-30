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

// The key in `Product Specific Bits Data` under which whether the prompt was
// triggered by a user gestured is recorded in the post-prompt HaTS survey.
const char kPermissionsPostPromptSurveyHadGestureKey[] = "HadGesture";
#endif

}  // namespace permissions
