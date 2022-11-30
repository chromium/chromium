// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_service.h"

#include <memory>

#include "chromecast/browser/cast_content_window_aura.h"

namespace chromecast {

std::unique_ptr<CastContentWindow> CastWebService::CreateWindow(
    mojom::CastWebViewParamsPtr params) {
  return std::make_unique<CastContentWindowAura>(std::move(params),
                                                 window_manager_);
}

}  // namespace chromecast
