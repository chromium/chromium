// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/default_session_controller.h"

namespace feature_engagement {

DefaultSessionController::DefaultSessionController() = default;
DefaultSessionController::~DefaultSessionController() = default;

bool DefaultSessionController::ShouldResetSession() {
  return false;
}

}  // namespace feature_engagement
