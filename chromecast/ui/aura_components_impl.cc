// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/aura_components_impl.h"

#include <utility>

#include "chromecast/ui/media_overlay_impl.h"

namespace chromecast {

AuraComponentsImpl::AuraComponentsImpl(CastWindowManager* cast_window_manager)
    : layout_provider_(std::make_unique<views::LayoutProvider>()) {
#if defined(ENABLE_MEDIA_OVERLAY)
  media_overlay_ = std::make_unique<MediaOverlayImpl>(cast_window_manager);
#endif
}

AuraComponentsImpl::~AuraComponentsImpl() = default;

// static
std::unique_ptr<AuraComponents> AuraComponents::Create(
    CastWindowManager* cast_window_manager) {
  return std::make_unique<AuraComponentsImpl>(cast_window_manager);
}

}  // namespace chromecast
