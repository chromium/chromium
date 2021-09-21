// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime_application.h"

namespace chromecast {

RuntimeApplication::RuntimeApplication() = default;

RuntimeApplication::~RuntimeApplication() = default;

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app) {
  return os << "Application '" << app.display_name() << "'' with id '"
            << app.app_id() << "' (session id '" << app.cast_session_id()
            << "')";
}

}  // namespace chromecast
