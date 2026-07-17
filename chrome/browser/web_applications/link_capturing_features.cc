// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/link_capturing_features.h"

#include "base/feature_list.h"
#include "base/strings/to_string.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace apps::features {

BASE_FEATURE(kUpdateAppStringsOnSettings, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPwaNavigationCapturingTestingOverride,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<::features::CapturingState>::Option
    kNavigationCapturingTestingOverrideParams[] = {
        {::features::CapturingState::kReimplDefaultOn, "reimpl_default_on"},
        {::features::CapturingState::kReimplOnViaClientMode,
         "reimpl_on_via_client_mode"}};

const base::FeatureParam<::features::CapturingState>
    kNavigationCapturingTestingOverrideState{
        &kPwaNavigationCapturingTestingOverride, "link_capturing_state",
        ::features::CapturingState::kReimplDefaultOn,
        &kNavigationCapturingTestingOverrideParams};

bool ShouldShowLinkCapturingUX() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return base::FeatureList::IsEnabled(::features::kPwaNavigationCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

::features::CapturingState GetNavigationCapturingDefaultState() {
  // 1. Check testing override feature flag first:
  if (base::FeatureList::IsEnabled(kPwaNavigationCapturingTestingOverride)) {
    return kNavigationCapturingTestingOverrideState.Get();
  }

  if (!base::FeatureList::IsEnabled(::features::kPwaNavigationCapturing)) {
    return ::features::CapturingState::kReimplDefaultOff;
  }

  // 2. Fall back to standard Finch seed / code default:
  return ::features::kNavigationCapturingDefaultState.Get();
}

bool IsNavigationCapturingOnByDefault() {
  if (!base::FeatureList::IsEnabled(::features::kPwaNavigationCapturing)) {
    return false;
  }
  const auto state = GetNavigationCapturingDefaultState();
  return state == ::features::CapturingState::kReimplDefaultOn ||
         state == ::features::CapturingState::kReimplOnViaClientMode;
}

}  // namespace apps::features
