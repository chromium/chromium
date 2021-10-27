// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HPS_HPS_DBUS_CLIENT_H_
#define CHROMEOS_DBUS_HPS_HPS_DBUS_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// D-Bus client for ambient presence sensing. Communicates with the Chrome OS
// presence daemon to allow for features that depend on user presence.
//
// TODO(crbug/1241706): clarify naming.
class COMPONENT_EXPORT(HPS) HpsDBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // See go/cros-hps-notify-ui-impl for event details.
    virtual void OnHpsNotifyChanged(bool state) = 0;

   protected:
    Observer();
  };

  using GetResultHpsNotifyCallback =
      base::OnceCallback<void(absl::optional<bool>)>;

  HpsDBusClient(const HpsDBusClient&) = delete;
  HpsDBusClient& operator=(const HpsDBusClient&) = delete;

  // Polls the HPS notify state.
  virtual void GetResultHpsNotify(GetResultHpsNotifyCallback cb) = 0;

  // Registers the given observer to receive HPS signals.
  virtual void AddObserver(Observer* observer) = 0;
  // Deregisters the given observer.
  virtual void RemoveObserver(Observer* observer) = 0;

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
