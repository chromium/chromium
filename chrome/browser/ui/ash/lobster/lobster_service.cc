// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lobster/lobster_service.h"

LobsterService::LobsterService() = default;

LobsterService::~LobsterService() = default;

LobsterSystemStateProvider* LobsterService::system_state_provider() {
  return &system_state_provider_;
}
