// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_

#include <unordered_map>

#include "base/memory/scoped_refptr.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"

namespace gfx {
struct VectorIcon;
}

// Returns the full set of encapsulated OmniboxPedal implementations.
std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool incognito, bool guest, bool testing);

// This utility method is used by `SharingHubIconView` and its related Pedal
// (Chrome Action button) in the omnibox. It returns the sharing hub icon,
// taking platform into account.
const gfx::VectorIcon& GetSharingHubVectorIcon();

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
