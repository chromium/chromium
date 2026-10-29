// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "chromecast/starboard/starboard_adapter_loader.h"

namespace chromecast {

// Proxy implementation for CastStarboardApiAdapter::GetInstance().
//
// Existing callers throughout the Cast codebase call
// CastStarboardApiAdapter::GetInstance() expecting a version-agnostic
// interface. This proxy routes those calls directly to StarboardAdapterLoader,
// which dynamically probes the partner Starboard library and initializes the
// in-process CastStarboardApiAdapterImpl for the detected Starboard version.
// static
CastStarboardApiAdapter* CastStarboardApiAdapter::GetInstance() {
  auto* adapter = StarboardAdapterLoader::GetInstance()->GetAdapter();
  CHECK(adapter);
  return adapter;
}

}  // namespace chromecast
