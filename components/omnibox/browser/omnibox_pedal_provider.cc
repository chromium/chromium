// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_provider.h"

#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_implementations.h"

OmniboxPedalProvider::OmniboxPedalProvider()
    : pedals_(GetPedalImplementations()) {}

OmniboxPedalProvider::~OmniboxPedalProvider() {}

OmniboxPedal* OmniboxPedalProvider::FindPedalMatch(
    const base::string16& match_text) const {
  // Right now Pedals are few and small, but if this linear search ever
  // encounters performance concerns, see crrev.com/c/1247223 for a ready made
  // optimization that quickly eliminates the vast majority of searches.
  for (const auto& pedal : pedals_) {
    if (pedal->IsTriggerMatch(match_text)) {
      return pedal.get();
    }
  }
  return nullptr;
}
