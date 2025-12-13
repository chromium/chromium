// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SWITCHES_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SWITCHES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace switches {

// When enabled, include account capabilities checks in choice screen
// eligibility evaluation.
COMPONENT_EXPORT(SEARCH_ENGINE_CHOICE_SWITCHES)
BASE_DECLARE_FEATURE(kChoiceScreenEligibilityCheckAccountCapabilities);

// When enabled, include management status checks in choice screen eligibility
// evaluation.
COMPONENT_EXPORT(SEARCH_ENGINE_CHOICE_SWITCHES)
BASE_DECLARE_FEATURE(kChoiceScreenEligibilityCheckManagementStatus);

}  // namespace switches

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SWITCHES_H_
