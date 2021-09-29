// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_
#define CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_

#include "chromeos/dbus/hps/hps_dbus_client.h"

namespace chromeos {

// Fake implementation of HpsDBusClient. This is currently a no-op fake.
class FakeHpsDBusClient : public HpsDBusClient {
 public:
  FakeHpsDBusClient();
  ~FakeHpsDBusClient() override;

  FakeHpsDBusClient(const FakeHpsDBusClient&) = delete;
  FakeHpsDBusClient& operator=(const FakeHpsDBusClient&) = delete;

  // HpsDBusClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetResultHpsNotify(GetResultHpsNotifyCallback cb) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_
