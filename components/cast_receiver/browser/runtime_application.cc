// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/runtime_application.h"

namespace cast_receiver {

RuntimeApplication::~RuntimeApplication() = default;

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app) {
  return os << "app_id=" << app.GetAppId() << " (" << app.GetDisplayName()
            << "), session_id=" << app.GetCastSessionId();
}

}  // namespace cast_receiver
