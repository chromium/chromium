// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HAMMERD_FAKE_HAMMERD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HAMMERD_FAKE_HAMMERD_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/hammerd/hammerd_client.h"

namespace ash {

class COMPONENT_EXPORT(HAMMERD) FakeHammerdClient : public HammerdClient {
 public:
  FakeHammerdClient();

  FakeHammerdClient(const FakeHammerdClient&) = delete;
  FakeHammerdClient& operator=(const FakeHammerdClient&) = delete;

  ~FakeHammerdClient() override;

  // Checks that a FakeHammerdClient instance was initialized and returns it.
  static FakeHammerdClient* Get();

  // HammerdClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Methods to simulate signals exposed by the hammerd service API.
  void FireBaseFirmwareNeedUpdateSignal();
  void FireBaseFirmwareUpdateStartedSignal();
  void FireBaseFirmwareUpdateSucceededSignal();
  void FireBaseFirmwareUpdateFailedSignal();
  void FirePairChallengeSucceededSignal(const std::vector<uint8_t>& base_id);
  void FirePairChallengeFailedSignal();
  void FireInvalidBaseConnectedSignal();

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HAMMERD_FAKE_HAMMERD_CLIENT_H_
