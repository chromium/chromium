// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HUMAN_PRESENCE_HUMAN_PRESENCE_DBUS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HUMAN_PRESENCE_HUMAN_PRESENCE_DBUS_CLIENT_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}

namespace ash {

// D-Bus client for human presence sensing. Communicates with the Chrome OS
// presence daemon to allow for features that depend on user presence.
//
// Use of this API is restricted by policy. Consult
// go/cros-pdd#bookmark=id.7emuxnhxv638 and Chrome OS Privacy before using.
class COMPONENT_EXPORT(HPS) HumanPresenceDBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // Called when the presence of a user starts or stops being detected.
    virtual void OnHpsSenseChanged(const hps::HpsResultProto&) = 0;

    // Called when the presence of a "snooper" looking over the user's shoulder
    // starts or stops being detected.
    virtual void OnHpsNotifyChanged(const hps::HpsResultProto&) = 0;

    // Called when the service starts or restarts.
    virtual void OnRestart() = 0;

    // Called when the service shuts down.
    virtual void OnShutdown() = 0;
  };

  using GetResultCallback =
      base::OnceCallback<void(std::optional<hps::HpsResultProto>)>;

  HumanPresenceDBusClient(const HumanPresenceDBusClient&) = delete;
  HumanPresenceDBusClient& operator=(const HumanPresenceDBusClient&) = delete;

  // Registers the given observer to receive human presence signals.
  virtual void AddObserver(Observer* observer) = 0;
  // Deregisters the given observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // D-Bus methods.
  // Polls the lock-on-leave state.
  virtual void GetResultHpsSense(GetResultCallback cb) = 0;
  // Polls the snooping protection state.
  virtual void GetResultHpsNotify(GetResultCallback cb) = 0;
  // Enables lock-on-leave in the service.
  virtual void EnableHpsSense(const hps::FeatureConfig& config) = 0;
  // Disables lock-on-leave in the service.
  virtual void DisableHpsSense() = 0;
  // Enables snooping protection in the service.
  virtual void EnableHpsNotify(const hps::FeatureConfig& config) = 0;
  // Disables snooping protection in the service.
  virtual void DisableHpsNotify() = 0;

  // Registers |callback| to run when the presence service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);
  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();
  // Destroys the global instance.
  static void Shutdown();
  // Returns the global instance which may be null if not initialized.
  static HumanPresenceDBusClient* Get();

 protected:
  // Initialize/Shutdown should be used instead.
  HumanPresenceDBusClient();
  virtual ~HumanPresenceDBusClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HUMAN_PRESENCE_HUMAN_PRESENCE_DBUS_CLIENT_H_
