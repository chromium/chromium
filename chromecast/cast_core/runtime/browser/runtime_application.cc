// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application.h"

namespace chromecast {

RuntimeApplication::~RuntimeApplication() = default;

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app) {
  return os << "app_id=" << app.GetAppId() << " (" << app.GetDisplayName()
            << "), session_id=" << app.GetCastSessionId();
}

}  // namespace chromecast
