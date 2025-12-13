// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_switches.h"

#include "base/feature_list.h"

namespace switches {

BASE_FEATURE(kChoiceScreenEligibilityCheckAccountCapabilities,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChoiceScreenEligibilityCheckManagementStatus,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace switches
