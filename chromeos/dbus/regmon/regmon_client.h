// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_REGMON_REGMON_CLIENT_H_
#define CHROMEOS_DBUS_REGMON_REGMON_CLIENT_H_

#include <list>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chromeos/dbus/regmon/regmon_service.pb.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// RegmonClient is used to communicate with the org.chromium.Regmond
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(REGMON) RegmonClient {
 public:
  using RecordPolicyViolationCallback = base::OnceCallback<void(
      const regmon::RecordPolicyViolationResponse response)>;

  // Interface with testing functionality. Accessed through
  // GetTestInterface(), only implemented in the fake implementation.
  class TestInterface {
   public:
    virtual std::list<int32_t> GetReportedHashCodes() = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  RegmonClient(const RegmonClient&) = delete;
  RegmonClient& operator=(const RegmonClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static RegmonClient* Get();

  // Regmon daemon D-Bus method calls. See org.chromium.Regmond.xml and
  // regmon_service.proto in Chromium OS code for the documentation of the
  // methods and request/response messages.
  virtual void RecordPolicyViolation(
      const regmon::RecordPolicyViolationRequest request) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  RegmonClient();
  virtual ~RegmonClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_REGMON_REGMON_CLIENT_H_
