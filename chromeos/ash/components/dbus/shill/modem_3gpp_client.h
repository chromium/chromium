// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_MODEM_3GPP_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_MODEM_3GPP_CLIENT_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace ash {

enum class CarrierLockResult {
  kSuccess,
  kUnknownError,
  kInvalidSignature,
  kInvalidImei,
  kInvalidTimeStamp,
  kNetworkListTooLarge,
  kAlgorithmNotSupported,
  kFeatureNotSupported,
  kDecodeOrParsingError,
  kNotInitialized,
  kOperationNotSupported,
};

// Modem3gppClient is used to communicate with the
// org.freedesktop.ModemManager1.Modem.Modem3gpp service.
// All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) Modem3gppClient {
 public:
  using CarrierLockCallback = base::OnceCallback<void(CarrierLockResult)>;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static Modem3gppClient* Get();

  Modem3gppClient(const Modem3gppClient&) = delete;
  Modem3gppClient& operator=(const Modem3gppClient&) = delete;

  // Calls SetCarrierLock method.
  // |callback| is called on method completion.
  virtual void SetCarrierLock(const std::string& service_name,
                              const dbus::ObjectPath& object_path,
                              const std::string& config,
                              CarrierLockCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  Modem3gppClient();
  virtual ~Modem3gppClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_MODEM_3GPP_CLIENT_H_
