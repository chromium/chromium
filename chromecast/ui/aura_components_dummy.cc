// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/aura_components.h"

namespace chromecast {

// static
std::unique_ptr<AuraComponents> AuraComponents::Create(
    CastWindowManager* cast_window_manager) {
  return std::make_unique<AuraComponents>();
}

}  // namespace chromecast
