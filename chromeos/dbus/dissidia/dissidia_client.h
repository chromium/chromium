// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DISSIDIA_DISSIDIA_CLIENT_H_
#define CHROMEOS_DBUS_DISSIDIA_DISSIDIA_CLIENT_H_

#include <cstdint>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "third_party/cros_system_api/dbus/dissidia/dbus-constants.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// DissidiaClient is used to communicate with the dissidia-daemon to perform
// image switching operations on Fjord devices. All methods should be called
// from the UI thread.
class COMPONENT_EXPORT(DISSIDIA) DissidiaClient {
 public:
  // Interface for observing signals from the dissidia daemon.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the Progress signal is received during an update.
    // |percent| is the overall progress percentage (0-100).
    // |stage| is the current stage (e.g., "download", "overall").
    virtual void OnProgress(int32_t percent, const std::string& stage) {}

    // Called when the Completed signal is received after an update finishes.
    // |success| is true if the update succeeded.
    // |error_code| provides a more specific error code.
    // |message| is a final status message.
    virtual void OnCompleted(bool success,
                             dissidia::CompletedErrorCode error_code,
                             const std::string& message) {}
  };

  // Callback for the PerformUpdate method.
  using PerformUpdateCallback =
      base::OnceCallback<void(dissidia::PerformUpdateStatus status,
                              const std::string& message)>;

  DissidiaClient(const DissidiaClient&) = delete;
  DissidiaClient& operator=(const DissidiaClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static DissidiaClient* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Initiates an update to the specified target image.
  // |target| is the image name, either "selphie" or "noctis".
  // |callback| receives the status and a descriptive message.
  virtual void PerformUpdate(const std::string& target,
                             PerformUpdateCallback callback) = 0;

 protected:
  DissidiaClient();
  virtual ~DissidiaClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DISSIDIA_DISSIDIA_CLIENT_H_
