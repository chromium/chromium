// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_
#define CHROMEOS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/rmad/rmad_client.h"

namespace chromeos {

class COMPONENT_EXPORT(RMAD) FakeRmadClient : public RmadClient {
 public:
  FakeRmadClient();
  FakeRmadClient(const FakeRmadClient&) = delete;
  FakeRmadClient& operator=(const FakeRmadClient&) = delete;
  ~FakeRmadClient() override;

  void GetCurrentState(
      DBusMethodCallback<rmad::GetStateReply> callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_
