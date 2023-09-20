// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_modem_3gpp_client.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

FakeModem3gppClient::FakeModem3gppClient() = default;

FakeModem3gppClient::~FakeModem3gppClient() = default;

void FakeModem3gppClient::SetCarrierLock(const std::string& service_name,
                                         const dbus::ObjectPath& object_path,
                                         const std::string& config,
                                         CarrierLockCallback callback) {
  carrier_lock_callback_ = std::move(callback);
}

void FakeModem3gppClient::CompleteSetCarrierLock(CarrierLockResult result) {
  std::move(carrier_lock_callback_).Run(result);
}

}  // namespace ash
