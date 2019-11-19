// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_

#include <memory>
#include <unordered_map>

#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_concepts.h"

class OmniboxPedalClearBrowsingData : public OmniboxPedal {
 public:
  OmniboxPedalClearBrowsingData();
#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif
};

class OmniboxPedalUpdateChrome : public OmniboxPedal {
 public:
  OmniboxPedalUpdateChrome();
  void Execute(ExecutionContext& context) const override;
  bool IsReadyToTrigger(
      const AutocompleteProviderClient& client) const override;
};

// Returns the full set of encapsulated OmniboxPedal implementations.
std::unordered_map<OmniboxPedalId, std::unique_ptr<OmniboxPedal>>
GetPedalImplementations();

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_IMPLEMENTATIONS_H_
