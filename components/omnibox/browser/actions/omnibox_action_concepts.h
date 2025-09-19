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
  EXTENSION_ACTION,

  // Takeover actions added to matches that are fulfilled via lens controller.
  CONTEXTUAL_SEARCH_FULFILLMENT,

  // Actions that enter @page scope for direct query or with lens selection.
  CONTEXTUAL_SEARCH_ASK_ABOUT_PAGE,  // Obsolete
  CONTEXTUAL_SEARCH_SELECT_REGION,   // Obsolete

  // An action to open lens with contextual search side panel ready.
  CONTEXTUAL_SEARCH_OPEN_LENS,

  // Keyword mode entry actions for builtin keywords, a.k.a. starter packs.
  // These are specified concretely instead of with one abstract keyword entry
  // action because most of the implementation consists of specifying details
  // like the labels, icons, etc. This also avoids the need for metric slicing.
  STARTER_PACK_BOOKMARKS,
  STARTER_PACK_HISTORY,
  STARTER_PACK_TABS,
  STARTER_PACK_AI_MODE,

  // Keep as a last item in the list, with ID one larger than the last valid
  // Action Id.
  LAST
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CONCEPTS_H_
