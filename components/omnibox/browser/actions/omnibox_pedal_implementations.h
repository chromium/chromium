// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_

#include <memory>
#include <unordered_map>

#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"

// Returns the full set of encapsulated OmniboxPedal implementations.
// |with_branding| specifies whether to include Google Chrome branded Pedals.
std::unordered_map<OmniboxPedalId, std::unique_ptr<OmniboxPedal>>
GetPedalImplementations(bool with_branding);

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
