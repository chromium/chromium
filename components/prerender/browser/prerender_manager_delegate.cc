// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/browser/prerender_manager_delegate.h"

namespace prerender {

PrerenderManagerDelegate::PrerenderManagerDelegate() = default;

void PrerenderManagerDelegate::MaybePreconnect(const GURL& url) {}

bool PrerenderManagerDelegate::IsNetworkPredictionPreferenceEnabled() {
  return true;
}

bool PrerenderManagerDelegate::IsPredictionDisabledDueToNetwork(Origin origin) {
  return false;
}

std::string PrerenderManagerDelegate::GetReasonForDisablingPrediction() {
  return std::string();
}

}  // namespace prerender