// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace critical_actions::features {

// Controls whether the Critical Action History service is enabled.
BASE_DECLARE_FEATURE(kCriticalActionHistory);

}  // namespace critical_actions::features

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_
