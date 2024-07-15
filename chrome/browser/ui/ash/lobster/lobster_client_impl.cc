// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lobster/lobster_client_impl.h"

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "chrome/browser/ui/ash/lobster/lobster_service.h"
#include "chrome/browser/ui/ash/lobster/lobster_system_state_provider.h"

LobsterClientImpl::LobsterClientImpl(LobsterService* service)
    : service_(service) {}

LobsterClientImpl::~LobsterClientImpl() = default;

void LobsterClientImpl::SetActiveSession(ash::LobsterSession* session) {
  service_->SetActiveSession(session);
}

ash::LobsterSystemState LobsterClientImpl::GetSystemState() {
  return service_->system_state_provider()->GetSystemState();
}
