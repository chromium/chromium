// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_features.h"

#include "base/feature_list.h"

namespace personal_context::features {

BASE_FEATURE(kPersonalContext, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kPersonalContextEnableFetchContext,
                   &kPersonalContext,
                   "personal_context_enable_fetch_context",
                   false);

BASE_FEATURE_PARAM(std::string,
                   kPersonalContextEligibleTiers,
                   &kPersonalContext,
                   "personal_context_eligible_tiers",
                   "1,2");

BASE_FEATURE_PARAM(std::string,
                   kContextMemoryServiceBaseUrl,
                   &kPersonalContext,
                   "context_memory_service_base_url",
                   "https://contextmemoryservice-pa.googleapis.com/v1");

BASE_FEATURE(kPersonalContextFirstRun, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPersonalContextFirstRunNoticePhase2,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPersonalContextFirstRunOptIn, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPersonalContextFirstRunEnabled() {
  return base::FeatureList::IsEnabled(kPersonalContextFirstRun) ||
         base::FeatureList::IsEnabled(kPersonalContextFirstRunNoticePhase2) ||
         base::FeatureList::IsEnabled(kPersonalContextFirstRunOptIn);
}

bool IsPersonalContextFirstRunNoticePhase2Enabled() {
  return base::FeatureList::IsEnabled(kPersonalContextFirstRunNoticePhase2);
}

bool IsPersonalContextFirstRunOptInEnabled() {
  return base::FeatureList::IsEnabled(kPersonalContextFirstRunOptIn);
}
}  // namespace personal_context::features
