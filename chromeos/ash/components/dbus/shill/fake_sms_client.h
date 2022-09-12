// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SMS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SMS_CLIENT_H_

#include <string>

#include "chromeos/ash/components/dbus/shill/sms_client.h"

namespace ash {

class FakeSMSClient : public SMSClient {
 public:
  FakeSMSClient();

  FakeSMSClient(const FakeSMSClient&) = delete;
  FakeSMSClient& operator=(const FakeSMSClient&) = delete;

  ~FakeSMSClient() override;

  // SMSClient overrides.
  void GetAll(const std::string& service_name,
              const dbus::ObjectPath& object_path,
              GetAllCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SMS_CLIENT_H_
