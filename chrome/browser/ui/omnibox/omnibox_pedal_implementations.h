// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_

#include <unordered_map>

#include "base/memory/scoped_refptr.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"

// Returns the full set of encapsulated OmniboxPedal implementations.
std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool incognito, bool testing);

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
