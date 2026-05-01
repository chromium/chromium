// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/features.h"

namespace password_manager::features {

BASE_FEATURE(kIOSStatelessFillDataFlow, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSPasswordAutoSubmission,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<AutoSubmissionType>::Option
    kAutoSubmissionTypeOptions[] = {
        {AutoSubmissionType::kDismissThenSubmit, "DismissThenSubmit"},
        {AutoSubmissionType::kSubmitThenDismiss, "SubmitThenDismiss"},
        {AutoSubmissionType::kDismissThenBlockThenSubmit,
         "DismissThenBlockThenSubmit"},
        {AutoSubmissionType::kScriptSubmit, "ScriptSubmit"}};

const base::FeatureParam<AutoSubmissionType> kAutoSubmissionTypeParam{
    &kIOSPasswordAutoSubmission, "auto-submission-type",
    AutoSubmissionType::kDismissThenSubmit, &kAutoSubmissionTypeOptions};

const base::FeatureParam<bool> kAutoSubmissionUseWaitPeriodParam{
    &kIOSPasswordAutoSubmission, "auto-submission-use-wait-period", false};

}  // namespace password_manager::features
