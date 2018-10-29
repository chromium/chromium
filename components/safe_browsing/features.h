// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_FEATURES_H_
#define COMPONENTS_SAFE_BROWSING_FEATURES_H_

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/values.h"
namespace base {
class ListValue;
}  // namespace base

namespace safe_browsing {
// Features list
extern const base::Feature kAdSamplerTriggerFeature;

// Controls the billing interstitial UI.
extern const base::Feature kBillingInterstitial;

extern const base::Feature kCheckByURLLoaderThrottle;

// Forces the chrome://reset-password page to be shown for review or testing
// purpose.
extern const base::Feature kForceEnableResetPasswordWebUI;

// Controls whether .rar files downloaded by the user are inspected for being
// unsafe.
extern const base::Feature kInspectDownloadedRarFiles;

// Controls the daily quota for the suspicious site trigger.
extern const base::Feature kSuspiciousSiteTriggerQuotaFeature;

// Specifies which non-resource HTML Elements to collect based on their tag and
// attributes. It's a single param containing a comma-separated list of pairs.
// For example: "tag1,id,tag1,height,tag2,foo" - this will collect elements with
// tag "tag1" that have attribute "id" or "height" set, and elements of tag
// "tag2" if they have attribute "foo" set. All tag names and attributes should
// be lower case.
extern const base::Feature kThreatDomDetailsTagAndAttributeFeature;

// Controls the daily quota for data collection triggers. It's a single param
// containing a comma-separated list of pairs. The format of the param is
// "T1,Q1,T2,Q2,...Tn,Qn", where Tx is a TriggerType and Qx is how many reports
// that trigger is allowed to send per day.
// TODO(crbug.com/744869): This param should be deprecated after ad sampler
// launch in favour of having a unique quota feature and param per trigger.
// Having a single shared feature makes it impossible to run multiple trigger
// trials simultaneously.
extern const base::Feature kTriggerThrottlerDailyQuotaFeature;

// Controls whether Chrome on Android uses locally cached blacklists.
extern const base::Feature kUseLocalBlacklistsV2;

base::ListValue GetFeatureStatusList();

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_FEATURES_H_
