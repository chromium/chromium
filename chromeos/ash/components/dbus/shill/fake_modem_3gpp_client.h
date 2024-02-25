// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_MODEM_3GPP_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_MODEM_3GPP_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "chromeos/ash/components/dbus/shill/modem_3gpp_client.h"
#include "dbus/object_path.h"

namespace ash {

class COMPONENT_EXPORT(SHILL_CLIENT) FakeModem3gppClient
    : public Modem3gppClient {
 public:
  FakeModem3gppClient();

  FakeModem3gppClient(const FakeModem3gppClient&) = delete;
  FakeModem3gppClient& operator=(const FakeModem3gppClient&) = delete;

  ~FakeModem3gppClient() override;

  // Modem3gppClient:
  void SetCarrierLock(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      const std::string& config,
                      CarrierLockCallback callback) override;

  void CompleteSetCarrierLock(CarrierLockResult result);

 private:
  CarrierLockCallback carrier_lock_callback_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_MODEM_3GPP_CLIENT_H_
