// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lobster/lobster_client_impl.h"

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "chrome/browser/ui/ash/lobster/lobster_system_state_provider.h"

LobsterClientImpl::LobsterClientImpl() = default;

LobsterClientImpl::~LobsterClientImpl() = default;

ash::LobsterSystemState LobsterClientImpl::GetSystemState() {
  return system_state_provider_.GetSystemState();
}
