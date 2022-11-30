// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/legacy_app_id_mapper.h"

namespace chromecast {
namespace {

// The AppID that should be returned for an unknown |id| by the below function.
constexpr uint32_t kAppUnknownFallback = 0x0;

}  // namespace

uint32_t MapLegacyAppId(const std::string& id) {
  return kAppUnknownFallback;
}

}  // namespace chromecast
