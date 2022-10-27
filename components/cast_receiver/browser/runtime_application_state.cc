// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/runtime_application_state.h"

namespace cast_receiver {

RuntimeApplicationState::~RuntimeApplicationState() = default;

std::ostream& operator<<(std::ostream& os,
                         const RuntimeApplicationState& app_state) {
  return os << "app_id=" << app_state.GetAppId() << " ("
            << app_state.GetDisplayName()
            << "), session_id=" << app_state.GetCastSessionId();
}

}  // namespace cast_receiver
