// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_DEBUG_FEATURES_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_DEBUG_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// The features in this namespace are not meant to be rolled out. They are only
// intended for manual debugging and testing purposes.
namespace personal_context::features::debug {

// Overrides the base URL of the Context Memory Service returned by
// `GetContextMemoryServiceBaseUrl`.
inline constexpr char kContextMemoryServiceBaseUrlSwitch[] =
    "context-memory-service-base-url";

// Overrides the base URL of the Context Memory Service returned by
// `GetContextMemoryServiceBaseUrl`.
BASE_DECLARE_FEATURE_PARAM(std::string, kContextMemoryServiceBaseUrlParam);

BASE_DECLARE_FEATURE(kPersonalContextForceEnablementState);
BASE_DECLARE_FEATURE_PARAM(int, kPersonalContextForceEnablementStateParam);

// When enabled, `AtMemoryQueryService` will always return local suggestions
// for the configured `MemoryDataType` (obtained via `AutofillDataProvider`)
// regardless of the query.
BASE_DECLARE_FEATURE(kMockPersonalContextResult);
BASE_DECLARE_FEATURE_PARAM(int, kMockPersonalContextResultTypeParam);

// When enabled, `PersonalContextFirstRunServiceImpl` will reset notice
// preferences on startup.
// TODO(b:532008442): When notice development has been completed and we don't
// expect future changes to the notices anymore, clean up this debug feature.
BASE_DECLARE_FEATURE(kPersonalContextResetNoticePrefsOnStartup);

}  // namespace personal_context::features::debug

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_DEBUG_FEATURES_H_
