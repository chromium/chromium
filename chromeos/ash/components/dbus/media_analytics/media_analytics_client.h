// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_MEDIA_ANALYTICS_MEDIA_ANALYTICS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_MEDIA_ANALYTICS_MEDIA_ANALYTICS_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/media_perception/media_perception.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
}

namespace ash {

// MediaAnalyticsClient is used to communicate with a media analytics process
// running outside of Chrome.
class COMPONENT_EXPORT(MEDIA_ANALYTICS_CLIENT) MediaAnalyticsClient {
 public:
  class Observer {
   public:
    // Called when DetectionSignal is received.
    virtual void OnDetectionSignal(
        const mri::MediaPerception& media_perception) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static MediaAnalyticsClient* Get();

  MediaAnalyticsClient(const MediaAnalyticsClient&) = delete;
  MediaAnalyticsClient& operator=(const MediaAnalyticsClient&) = delete;

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the media analytics process state.
  virtual void GetState(chromeos::DBusMethodCallback<mri::State> callback) = 0;

  // Sets the media analytics process state. |state.status| is expected to be
  // set.
  virtual void SetState(const mri::State& state,
                        chromeos::DBusMethodCallback<mri::State> callback) = 0;

  // API for getting diagnostic information from the media analytics process
  // over D-Bus as a Diagnostics proto message.
  virtual void GetDiagnostics(
      chromeos::DBusMethodCallback<mri::Diagnostics> callback) = 0;

  // Bootstrap the Mojo connection between Chrome and the media analytics
  // process. Should pass in the file descriptor for the child end of the Mojo
  // pipe.
  virtual void BootstrapMojoConnection(
      base::ScopedFD file_descriptor,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Factory function, creates new instance and returns ownership.
  // For normal usage, access the singleton via DbusThreadManager::Get().
  static MediaAnalyticsClient* Create();

 protected:
  // Initialize/Shutdown should be used instead.
  MediaAnalyticsClient();
  virtual ~MediaAnalyticsClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_MEDIA_ANALYTICS_MEDIA_ANALYTICS_CLIENT_H_
