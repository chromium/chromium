// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HUMAN_PRESENCE_FAKE_HUMAN_PRESENCE_DBUS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HUMAN_PRESENCE_FAKE_HUMAN_PRESENCE_DBUS_CLIENT_H_

#include <optional>

#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"

namespace ash {

// Fake implementation of HumanPresenceDBusClient. Allows callers to set a
// response value and count the number of calls to GetResultHpsNotify.
class COMPONENT_EXPORT(HPS) FakeHumanPresenceDBusClient
    : public HumanPresenceDBusClient {
 public:
  FakeHumanPresenceDBusClient();
  ~FakeHumanPresenceDBusClient() override;

  FakeHumanPresenceDBusClient(const FakeHumanPresenceDBusClient&) = delete;
  FakeHumanPresenceDBusClient& operator=(const FakeHumanPresenceDBusClient&) =
      delete;

  // Returns the fake global instance if initialized. May return null.
  static FakeHumanPresenceDBusClient* Get();

  // HumanPresenceDBusClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetResultHpsSense(GetResultCallback cb) override;
  void GetResultHpsNotify(GetResultCallback cb) override;
  void EnableHpsSense(const hps::FeatureConfig& config) override;
  void DisableHpsSense() override;
  void EnableHpsNotify(const hps::FeatureConfig& config) override;
  void DisableHpsNotify() override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback cb) override;

  // Methods for co-ordinating GetResultHpsNotify calls in tests.

  void set_hps_notify_result(std::optional<hps::HpsResultProto> result) {
    hps_notify_result_ = result;
  }
  void set_hps_sense_result(std::optional<hps::HpsResultProto> result) {
    hps_sense_result_ = result;
  }

  int hps_notify_count() const { return hps_notify_count_; }
  int hps_sense_count() const { return hps_sense_count_; }

  // Methods for co-ordinating WaitForServiceToBeAvailable calls in tests.
  void set_hps_service_is_available(bool is_available) {
    hps_service_is_available_ = is_available;
  }

  // Methods for co-ordinating notify enable/disable in tests.
  int enable_hps_notify_count() const { return enable_hps_notify_count_; }
  int disable_hps_notify_count() const { return disable_hps_notify_count_; }

  // Methods for co-ordinating sense enable/disable in tests.
  int enable_hps_sense_count() const { return enable_hps_sense_count_; }
  int disable_hps_sense_count() const { return disable_hps_sense_count_; }

  // Simulte HpsService restart.
  void Restart();

  // Simulte HpsService shutdown.
  void Shutdown();

  // Resets all parameters; used in unittests.
  void Reset();

 private:
  std::optional<hps::HpsResultProto> hps_notify_result_;
  std::optional<hps::HpsResultProto> hps_sense_result_;
  int hps_notify_count_ = 0;
  int hps_sense_count_ = 0;
  int enable_hps_notify_count_ = 0;
  int disable_hps_notify_count_ = 0;
  int enable_hps_sense_count_ = 0;
  int disable_hps_sense_count_ = 0;
  bool hps_service_is_available_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HUMAN_PRESENCE_FAKE_HUMAN_PRESENCE_DBUS_CLIENT_H_
