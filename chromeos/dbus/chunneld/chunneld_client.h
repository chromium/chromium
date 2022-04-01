// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CHUNNELD_CHUNNELD_CLIENT_H_
#define CHROMEOS_DBUS_CHUNNELD_CHUNNELD_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/chunneld/chunneld_service.pb.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// ChunneldClient is used to communicate with chunneld and monitor chunneld.
class COMPONENT_EXPORT(CHROMEOS_DBUS_CHUNNELD) ChunneldClient
    : public DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when Chunneld service exits.
    virtual void ChunneldServiceStopped() = 0;
    // Called when Chunneld service is either started or restarted.
    virtual void ChunneldServiceStarted() = 0;
  };

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  ~ChunneldClient() override;

  ChunneldClient(const ChunneldClient&) = delete;
  ChunneldClient& operator=(const ChunneldClient&) = delete;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ChunneldClient> Create();

  // Registers |callback| to run when the Concierge service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

 protected:
  // Create() should be used instead.
  ChunneldClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CHUNNELD_CHUNNELD_CLIENT_H_
