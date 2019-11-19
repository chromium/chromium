// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/location_bar_helper.h"

#include <memory>

#include "components/omnibox/browser/location_bar_model_impl.h"

class LocationBarModelDelegate;

namespace vr {

namespace {

// This number is arbitrary. For VR, use a number smaller than desktop's 32K, as
// the URL indicator does not show long URLs.
constexpr int kMaxURLDisplayChars = 1024;

}  // namespace

LocationBarHelper::LocationBarHelper(BrowserUiInterface* ui,
                                     LocationBarModelDelegate* delegate)
    : ui_(ui),
      location_bar_model_(
          std::make_unique<LocationBarModelImpl>(delegate,
                                                 kMaxURLDisplayChars)) {}

LocationBarHelper::~LocationBarHelper() {}

void LocationBarHelper::Update() {
  LocationBarState state(location_bar_model_->GetURL(),
                         location_bar_model_->GetSecurityLevel(),
                         &location_bar_model_->GetVectorIcon(),
                         location_bar_model_->ShouldDisplayURL(),
                         location_bar_model_->IsOfflinePage());

  if (current_state_ == state)
    return;
  current_state_ = state;
  ui_->SetLocationBarState(state);
}

}  // namespace vr
