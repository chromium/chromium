// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_modem_messaging_client.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "dbus/object_path.h"

namespace chromeos {

FakeModemMessagingClient::FakeModemMessagingClient() = default;
FakeModemMessagingClient::~FakeModemMessagingClient() = default;

void FakeModemMessagingClient::SetSmsReceivedHandler(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const SmsReceivedHandler& handler) {
  sms_received_handler_ = handler;
}

void FakeModemMessagingClient::ResetSmsReceivedHandler(
    const std::string& service_name,
    const dbus::ObjectPath& object_path) {
  sms_received_handler_.Reset();
}

void FakeModemMessagingClient::Delete(const std::string& service_name,
                                      const dbus::ObjectPath& object_path,
                                      const dbus::ObjectPath& sms_path,
                                      VoidDBusMethodCallback callback) {
  std::vector<dbus::ObjectPath>::iterator it(
      find(message_paths_.begin(), message_paths_.end(), sms_path));
  if (it != message_paths_.end())
    message_paths_.erase(it);
  std::move(callback).Run(true);
}

void FakeModemMessagingClient::List(const std::string& service_name,
                                    const dbus::ObjectPath& object_path,
                                    ListCallback callback) {
  // This entire FakeModemMessagingClient is for testing.
  // Calling List with |service_name| equal to "AddSMS" allows unit
  // tests to confirm that the sms_received_handler is functioning.
  if (service_name == "AddSMS") {
    const dbus::ObjectPath kSmsPath("/SMS/0");
    message_paths_.push_back(kSmsPath);
    if (!sms_received_handler_.is_null())
      sms_received_handler_.Run(kSmsPath, true);
    std::move(callback).Run({});
  } else {
    std::move(callback).Run(message_paths_);
  }
}

}  // namespace chromeos
