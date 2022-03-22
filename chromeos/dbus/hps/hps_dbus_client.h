// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HPS_HPS_DBUS_CLIENT_H_
#define CHROMEOS_DBUS_HPS_HPS_DBUS_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "dbus/object_proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// D-Bus client for ambient presence sensing. Communicates with the Chrome OS
// presence daemon to allow for features that depend on user presence.
//
// Use of this API is restricted by policy. Consult
// go/cros-pdd#bookmark=id.7emuxnhxv638 and Chrome OS Privacy before using.
//
// TODO(crbug/1241706): clarify naming.
class COMPONENT_EXPORT(HPS) HpsDBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // Called when the presence of a user starts or stops being detected.
    virtual void OnHpsSenseChanged(hps::HpsResult state) = 0;

    // Called when the presence of a "snooper" looking over the user's shoulder
    // starts or stops being detected.
    virtual void OnHpsNotifyChanged(hps::HpsResult state) = 0;

    // Called when the service starts or restarts.
    virtual void OnRestart() = 0;

    // Called when the service shuts down.
    virtual void OnShutdown() = 0;
  };

  using GetResultCallback =
      base::OnceCallback<void(absl::optional<hps::HpsResult>)>;

  HpsDBusClient(const HpsDBusClient&) = delete;
  HpsDBusClient& operator=(const HpsDBusClient&) = delete;

  // Registers the given observer to receive HPS signals.
  virtual void AddObserver(Observer* observer) = 0;
  // Deregisters the given observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // D-Bus methods.
  // Polls the HPS sense state.
  virtual void GetResultHpsSense(GetResultCallback cb) = 0;
  // Polls the HPS notify state.
  virtual void GetResultHpsNotify(GetResultCallback cb) = 0;
  // Enables HpsSense in HpsService.
  virtual void EnableHpsSense(const hps::FeatureConfig& config) = 0;
  // Disables HpsSense in HpsService.
  virtual void DisableHpsSense() = 0;
  // Enables HpsNotify in HpsService.
  virtual void EnableHpsNotify(const hps::FeatureConfig& config) = 0;
  // Disables HpsNotify in HpsService.
  virtual void DisableHpsNotify() = 0;

  // Registers |callback| to run when the HpsService becomes available.
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
  static HpsDBusClient* Get();

 protected:
  // Initialize/Shutdown should be used instead.
  HpsDBusClient();
  virtual ~HpsDBusClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HPS_HPS_DBUS_CLIENT_H_
