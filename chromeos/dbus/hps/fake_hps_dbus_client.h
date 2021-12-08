// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_
#define CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_

#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// Fake implementation of HpsDBusClient. Allows callers to set a response value
// and count the number of calls to GetResultHpsNotify.
class COMPONENT_EXPORT(HPS) FakeHpsDBusClient : public HpsDBusClient {
 public:
  FakeHpsDBusClient();
  ~FakeHpsDBusClient() override;

  FakeHpsDBusClient(const FakeHpsDBusClient&) = delete;
  FakeHpsDBusClient& operator=(const FakeHpsDBusClient&) = delete;

  // Returns the fake global instance if initialized. May return null.
  static FakeHpsDBusClient* Get();

  // HpsDBusClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetResultHpsNotify(GetResultHpsNotifyCallback cb) override;
  void EnableHpsSense(const hps::FeatureConfig& config) override;
  void DisableHpsSense() override;
  void EnableHpsNotify(const hps::FeatureConfig& config) override;
  void DisableHpsNotify() override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback cb) override;

  // Methods for co-ordinating GetResultHpsNotify calls in tests.

  void set_hps_notify_result(absl::optional<bool> result) {
    hps_notify_result_ = result;
  }

  int hps_notify_count() const { return hps_notify_count_; }

  // Methods for co-ordinating WaitForServiceToBeAvailable calls in tests.
  void set_hps_service_is_available(bool is_available) {
    hps_service_is_available_ = is_available;
  }

 private:
  absl::optional<bool> hps_notify_result_;
  int hps_notify_count_ = 0;
  bool hps_service_is_available_ = false;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_
