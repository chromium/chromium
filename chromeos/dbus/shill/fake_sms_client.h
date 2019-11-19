// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_FAKE_SMS_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_FAKE_SMS_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "chromeos/dbus/shill/sms_client.h"

namespace chromeos {

class FakeSMSClient : public SMSClient {
 public:
  FakeSMSClient();
  ~FakeSMSClient() override;

  // SMSClient overrides.
  void GetAll(const std::string& service_name,
              const dbus::ObjectPath& object_path,
              GetAllCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSMSClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_FAKE_SMS_CLIENT_H_
