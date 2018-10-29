// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_

#include <memory>
#include <vector>

class OmniboxPedal;

// Returns the full set of encapsulated OmniboxPedal implementations.
std::vector<std::unique_ptr<OmniboxPedal>> GetPedalImplementations();

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
