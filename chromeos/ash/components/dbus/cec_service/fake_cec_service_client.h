// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CEC_SERVICE_FAKE_CEC_SERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CEC_SERVICE_FAKE_CEC_SERVICE_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"

namespace ash {

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
  void Init(dbus::Bus* bus) override;

  int stand_by_call_count() const { return stand_by_call_count_; }
  int wake_up_call_count() const { return wake_up_call_count_; }

  void set_tv_power_states(const std::vector<PowerState>& power_states) {
    tv_power_states_ = power_states;
  }
  const std::vector<PowerState>& tv_power_states() const {
    return tv_power_states_;
  }
  void reset() {
    stand_by_call_count_ = 0;
    wake_up_call_count_ = 0;
    tv_power_states_.clear();
  }

 private:
  void SetDisplayPowerState(PowerState new_state);

  int stand_by_call_count_ = 0;
  int wake_up_call_count_ = 0;

  std::vector<PowerState> tv_power_states_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CEC_SERVICE_FAKE_CEC_SERVICE_CLIENT_H_
