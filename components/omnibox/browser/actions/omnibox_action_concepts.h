// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CONCEPTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CONCEPTS_H_

#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"

// Unique identifiers for actions that aren't pedals, e.g. the history clusters
// action. Do not remove or reuse values. The values here must remain disjoint
// with the OmniboxPedalId enum so they start at 10000.
//
// Automatically generate a corresponding Java enum:
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.omnibox.action
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: OmniboxActionType
enum class OmniboxActionId {
  FIRST = 10000,
  HISTORY_CLUSTERS = 10001,

  // Last value, used to track the upper bound. This intentionally does not have
  // an assigned value to ensure that it's always 1 greater than the last
  // assigned value.
  LAST
};

static_assert(static_cast<int32_t>(OmniboxActionId::FIRST) >
                  static_cast<int32_t>(OmniboxPedalId::TOTAL_COUNT),
              "OmniboxPedalId and OmniboxActionId must remain disjoint");

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CONCEPTS_H_
