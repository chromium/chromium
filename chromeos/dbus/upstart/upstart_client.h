// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UPSTART_UPSTART_CLIENT_H_
#define CHROMEOS_DBUS_UPSTART_UPSTART_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// UpstartClient is used to communicate with the com.ubuntu.Upstart
// sevice. All methods should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(UPSTART_CLIENT) UpstartClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}
    // Called when the ARC is stopped after it had already started.
    virtual void ArcStopped() {}
  };

  virtual ~UpstartClient();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static UpstartClient* Get();

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Starts an Upstart job.
  // |job|: Name of Upstart job.
  // |upstart_env|: List of upstart environment variables to be passed to the
  // upstart service.
  // |callback|: Called with a response.
  virtual void StartJob(const std::string& job,
                        const std::vector<std::string>& upstart_env,
                        VoidDBusMethodCallback callback) = 0;

  // Stops an Upstart job.
  // |job|: Name of Upstart job.
  // |upstart_env|: List of upstart environment variables to be passed to the
  // upstart service.
  // |callback|: Called with a response.
  virtual void StopJob(const std::string& job,
                       const std::vector<std::string>& upstart_env,
                       VoidDBusMethodCallback callback) = 0;

  // Starts authpolicyd.
  virtual void StartAuthPolicyService() = 0;

  // Restarts authpolicyd.
  virtual void RestartAuthPolicyService() = 0;

  // Starts the media analytics process.
  // |upstart_env|: List of upstart environment variables to be passed to the
  // upstart service.
  virtual void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                                   VoidDBusMethodCallback callback) = 0;

  // Restarts the media analytics process.
  virtual void RestartMediaAnalytics(VoidDBusMethodCallback callback) = 0;

  // Stops the media analytics process.
  virtual void StopMediaAnalytics() = 0;

  // Provides an interface for stopping the media analytics process.
  virtual void StopMediaAnalytics(VoidDBusMethodCallback callback) = 0;

  // Start wilco DTC services.
  virtual void StartWilcoDtcService(VoidDBusMethodCallback callback) = 0;

  // Stops wilco DTC services.
  virtual void StopWilcoDtcService(VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  UpstartClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(UpstartClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_UPSTART_UPSTART_CLIENT_H_
