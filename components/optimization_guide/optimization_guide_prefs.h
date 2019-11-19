// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PREFS_H_

#include "base/macros.h"

class PrefRegistrySimple;

namespace optimization_guide {
namespace prefs {

extern const char kHintsFetcherLastFetchAttempt[];
extern const char kHintsFetcherDataSaverTopHostBlacklist[];
extern const char kHintsFetcherDataSaverTopHostBlacklistState[];
extern const char kTimeBlacklistLastInitialized[];
extern const char
    kHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore[];
extern const char kHintsFetcherHostsSuccessfullyFetched[];
extern const char kPendingHintsProcessingVersion[];

// State of |HintsFetcherTopHostsBlacklist|. The blacklist begins in
// kNotInitialized and transitions to kInitialized after
// InitializeHintsFetcherTopHostBlack() is called. When the blacklist no longer
// contains any hosts, the state transitions to kEmpty.
enum class HintsFetcherTopHostBlacklistState {
  kNotInitialized = 0,
  kInitialized = 1,
  kEmpty = 2,
  kMaxValue = kEmpty,
};

// Registers the optimization guide's prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PREFS_H_
