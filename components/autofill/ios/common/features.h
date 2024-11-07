// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_
#define COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

BASE_DECLARE_FEATURE(kAutofillDynamicallyLoadsFieldsForAddressInput);
BASE_DECLARE_FEATURE(kAutofillIsolatedWorldForJavascriptIos);
BASE_DECLARE_FEATURE(kAutofillPaymentsSheetV2Ios);
BASE_DECLARE_FEATURE(kAutofillStickyInfobarIos);

BASE_DECLARE_FEATURE(kAutofillThrottleDocumentFormScanIos);
extern const base::FeatureParam<int> kAutofillDocumentFormScanPeriodMs;

BASE_DECLARE_FEATURE(kAutofillThrottleDocumentFormScanForceFirstScanIos);

BASE_DECLARE_FEATURE(kAutofillThrottleFilteredDocumentFormScanIos);
extern const base::FeatureParam<int> kAutofillFilteredDocumentFormScanPeriodMs;

#endif  // COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_
