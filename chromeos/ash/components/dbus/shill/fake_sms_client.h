// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SMS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SMS_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/shill/sms_client.h"
#include "dbus/object_path.h"

namespace ash {

class COMPONENT_EXPORT(SHILL_CLIENT) FakeSMSClient : public SMSClient {
 public:
  static const char kNumber[];
  static const char kTimestamp[];

  FakeSMSClient();

  FakeSMSClient(const FakeSMSClient&) = delete;
  FakeSMSClient& operator=(const FakeSMSClient&) = delete;

  ~FakeSMSClient() override;

  // Completes the pending GetAll() callback. Simulates GetAll() being
  // asynchronous.
  void CompleteGetAll();

  // SMSClient overrides.
  void GetAll(const std::string& service_name,
              const dbus::ObjectPath& object_path,
              GetAllCallback callback) override;

 private:
  GetAllCallback pending_get_all_callback_;
  dbus::ObjectPath pending_get_all_object_path_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SMS_CLIENT_H_
