// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_UPSTART_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_UPSTART_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
}

namespace ash {

// UpstartClient is used to communicate with the com.ubuntu.Upstart
// sevice. All methods should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(UPSTART_CLIENT) UpstartClient {
 public:
  // Error name returned by Upstart when it fails to start a job because it's
  // already started.
  static const char kAlreadyStartedError[];

  UpstartClient(const UpstartClient&) = delete;
  UpstartClient& operator=(const UpstartClient&) = delete;

  virtual ~UpstartClient();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static UpstartClient* Get();

  // Starts an Upstart job.
  // |job|: Name of Upstart job.
  // |upstart_env|: List of upstart environment variables to be passed to the
  // upstart service.
  // |callback|: Called with a response.
  virtual void StartJob(const std::string& job,
                        const std::vector<std::string>& upstart_env,
                        chromeos::VoidDBusMethodCallback callback) = 0;

  // Starts an Upstart job with a timeout.
  // |job|: Name of Upstart job.
  // |upstart_env|: List of upstart environment variables to be passed to the
  // upstart service.
  // |callback|: Called with a response.
  // |timeout_ms|: Duration in milliseconds to wait for a response.
  // A value of TIMEOUT_INFINITE can be used for no timeout.
  virtual void StartJobWithTimeout(const std::string& job,
                                   const std::vector<std::string>& upstart_env,
                                   chromeos::VoidDBusMethodCallback callback,
                                   int timeout_ms) = 0;

  // Does the same thing as StartJob(), but the callback is run with error
  // details on failures.
  // See https://dbus.freedesktop.org/doc/dbus-specification.html to see what
  // error name and error message are.
  //
  // NOTE: Any of error_name and error_message can be null even if success ==
  // false. D-Bus method calls can fail without returning an error response
  // (e.g. when the D-Bus connection itself is disconnected).
  using StartJobWithErrorDetailsCallback =
      base::OnceCallback<void(bool success,
                              std::optional<std::string> error_name,
                              std::optional<std::string> error_message)>;
  virtual void StartJobWithErrorDetails(
      const std::string& job,
      const std::vector<std::string>& upstart_env,
      StartJobWithErrorDetailsCallback callback) = 0;

  // Stops an Upstart job.
  // |job|: Name of Upstart job.
  // |upstart_env|: List of upstart environment variables to be passed to the
  // upstart service.
  // |callback|: Called with a response.
  virtual void StopJob(const std::string& job,
                       const std::vector<std::string>& upstart_env,
                       chromeos::VoidDBusMethodCallback callback) = 0;

  // Starts the media analytics process.
  // |upstart_env|: List of upstart environment variables to be passed to the
  // upstart service.
  virtual void StartMediaAnalytics(
      const std::vector<std::string>& upstart_env,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Restarts the media analytics process.
  virtual void RestartMediaAnalytics(
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Stops the media analytics process.
  virtual void StopMediaAnalytics() = 0;

  // Provides an interface for stopping the media analytics process.
  virtual void StopMediaAnalytics(
      chromeos::VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  UpstartClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_UPSTART_CLIENT_H_
