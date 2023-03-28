// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_PING_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_PING_MANAGER_H_

#include "chromeos/ash/components/phonehub/ping_manager.h"

namespace ash::phonehub {

class FakePingManager : public PingManager {
 public:
  FakePingManager();
  ~FakePingManager() override;

  // PingManager:
  void SendPingRequest() override;

  void OnPingResponseReceived();

  int GetNumPingRequests() const;
  bool GetIsWaitingForResponse() const;

 private:
  // PingManager:
  void Reset() override;

  int num_ping_requests_ = 0;
  bool is_waiting_for_response_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_PING_MANAGER_H_
