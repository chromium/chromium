// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace critical_actions::features {

// Controls whether the Critical Action History service is enabled.
BASE_DECLARE_FEATURE(kCriticalActionHistory);

// Maximum number of recent navigation entries retained in the LRU cache.
extern const base::FeatureParam<int> kMaxNavigationCacheCapacity;

// Maximum number of recent visit-to-critical-actions entries retained in the
// LRU cache for redirect tracking.
extern const base::FeatureParam<int> kMaxVisitToActionsCacheCapacity;

// Maximum number of task-to-conversation entries retained in the LRU cache.
extern const base::FeatureParam<int> kMaxTaskToConversationCacheCapacity;

// Maximum number of task-to-critical-action-id entries retained in the LRU
// cache.
extern const base::FeatureParam<int> kMaxTaskToCriticalActionIdsCacheCapacity;

// Maximum number of task-to-anchor entries retained in the LRU cache.
extern const base::FeatureParam<int> kMaxTaskToAnchorCacheCapacity;

// Parameter controlling whether to replace "Review Gemini Activity" with
// "Go to Gemini chat" in the history menu. Prefer `IsChatLinkoutsEnabled()`
// over reading `.Get()` directly so `kCriticalActionHistory` is also checked.
extern const base::FeatureParam<bool> kEnableChatLinkouts;

// Parameter controlling whether chat linkouts target the specific
// action-level turn/message in the chat. Only effective when both
// `kCriticalActionHistory` and `kEnableChatLinkouts` are enabled; prefer
// `IsActionLevelChatLinkoutsEnabled()` over reading `.Get()` directly.
extern const base::FeatureParam<bool> kEnableActionLevelChatLinkouts;

// Returns true if Critical Action History and chat linkouts are both enabled.
bool IsChatLinkoutsEnabled();

// Returns true if action-level chat linkouts are enabled. Effective only when
// `IsChatLinkoutsEnabled()` (i.e. both `kCriticalActionHistory` and
// `kEnableChatLinkouts`) is also true.
bool IsActionLevelChatLinkoutsEnabled();

}  // namespace critical_actions::features

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_
