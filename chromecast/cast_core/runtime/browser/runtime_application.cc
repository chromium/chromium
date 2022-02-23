// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application.h"

namespace chromecast {

RuntimeApplication::~RuntimeApplication() = default;

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app) {
  return os << "app_id=" << app.GetAppConfig().app_id() << " ("
            << app.GetAppConfig().display_name()
            << "), session_id=" << app.GetCastSessionId()
            << ", url=" << app.GetApplicationUrl();
}

}  // namespace chromecast
