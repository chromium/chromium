// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CONCEPTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CONCEPTS_H_

#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"

// Unique identifiers for actions that aren't pedals, e.g. the history clusters
// action. Do not remove or reuse values.
//
// Automatically generate a corresponding Java enum:
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox.action
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: OmniboxActionId
enum class OmniboxActionId {
  UNKNOWN = 0,
  PEDAL,
  HISTORY_CLUSTERS,
  ACTION_IN_SUGGEST,
  TAB_SWITCH,
  ANSWER_ACTION,
  // Keep as a last item in the list, with ID one larger than the last valid
  // Action Id.
  LAST
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CONCEPTS_H_
