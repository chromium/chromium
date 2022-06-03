// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_FEATURES_H_
#define COMPONENTS_BROWSER_WATCHER_FEATURES_H_

#include "base/feature_list.h"

namespace browser_watcher {

// Enables activity tracking and extending crash reports with structured
// high-level program state.
extern const base::Feature kExtendedCrashReportingFeature;

// Name of an experiment parameter that controls whether to record browser
// activity in-memory only.
extern const char kInMemoryOnlyParam[];

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_FEATURES_H_
