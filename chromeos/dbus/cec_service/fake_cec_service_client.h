// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CEC_SERVICE_FAKE_CEC_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_CEC_SERVICE_FAKE_CEC_SERVICE_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "chromeos/dbus/cec_service/cec_service_client.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS_CEC_SERVICE) FakeCecServiceClient
    : public CecServiceClient {
 public:
  FakeCecServiceClient();

  FakeCecServiceClient(const FakeCecServiceClient&) = delete;
  FakeCecServiceClient& operator=(const FakeCecServiceClient&) = delete;

  ~FakeCecServiceClient() override;

  // CecServiceClient
  void SendStandBy() override;
  void SendWakeUp() override;
  void QueryDisplayCecPowerState(
      CecServiceClient::PowerStateCallback callback) override;

  int stand_by_call_count() const { return stand_by_call_count_; }
  int wake_up_call_count() const { return wake_up_call_count_; }

  void set_tv_power_states(const std::vector<PowerState>& power_states) {
    tv_power_states_ = power_states;
  }
  const std::vector<PowerState>& tv_power_states() const {
    return tv_power_states_;
  }

 protected:
  void Init(dbus::Bus* bus) override;

 private:
  void SetDisplayPowerState(PowerState new_state);

  int stand_by_call_count_ = 0;
  int wake_up_call_count_ = 0;

  std::vector<PowerState> tv_power_states_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CEC_SERVICE_FAKE_CEC_SERVICE_CLIENT_H_
