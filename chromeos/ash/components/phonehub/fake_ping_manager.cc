// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_ping_manager.h"

#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash::phonehub {

const proto::PingRequest kDefaultPingRequest;

FakePingManager::FakePingManager() = default;

FakePingManager::~FakePingManager() = default;

void FakePingManager::SendPingRequest() {
  ++num_ping_requests_;
  is_waiting_for_response_ = true;
}

void FakePingManager::OnPingResponseReceived() {
  is_waiting_for_response_ = false;
}

int FakePingManager::GetNumPingRequests() const {
  return num_ping_requests_;
}

bool FakePingManager::GetIsWaitingForResponse() const {
  return is_waiting_for_response_;
}

void FakePingManager::Reset() {
  is_waiting_for_response_ = false;
}

}  // namespace ash::phonehub
