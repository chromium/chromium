// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_GEOLOCATION_BRIDGE_H_
#define CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_GEOLOCATION_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "url/gurl.h"

class InstalledWebappGeolocationContext;

// Implements the Geolocation Mojo interface.
class InstalledWebappGeolocationBridge : public device::mojom::Geolocation {
 public:
  // |context| must outlive this object.
  InstalledWebappGeolocationBridge(
      mojo::PendingReceiver<device::mojom::Geolocation> receiver,
      const GURL& origin,
      InstalledWebappGeolocationContext* context);
  InstalledWebappGeolocationBridge(const InstalledWebappGeolocationBridge&) =
      delete;
  InstalledWebappGeolocationBridge& operator=(
      const InstalledWebappGeolocationBridge&) = delete;
  ~InstalledWebappGeolocationBridge() override;

  // Starts listening for updates.
  void StartListeningForUpdates();
  void StopUpdates();

  // Enables and disables geolocation override.
  void SetOverride(device::mojom::GeopositionResultPtr result);
  void ClearOverride();

  // Handles permission revocations during active use.
  void OnPermissionRevoked();

  // Called by JNI on its thread looper.
  void OnNewLocationAvailable(JNIEnv* env,
                              jdouble latitude,
                              jdouble longitude,
                              jdouble time_stamp,
                              jboolean has_altitude,
                              jdouble altitude,
                              jboolean has_accuracy,
                              jdouble accuracy,
                              jboolean has_heading,
                              jdouble heading,
                              jboolean has_speed,
                              jdouble speed);
  void OnNewErrorAvailable(JNIEnv* env, std::string& message);

  const GURL& url() { return url_; }

 private:
  // device::mojom::Geolocation:
  void SetHighAccuracy(bool high_accuracy) override;
  void QueryNextPosition(QueryNextPositionCallback callback) override;

  void OnConnectionError();

  void OnLocationUpdate(device::mojom::GeopositionResultPtr result);
  void ReportCurrentPosition();

  // Owns this object.
  raw_ptr<InstalledWebappGeolocationContext> context_;

  // The callback passed to QueryNextPosition.
  QueryNextPositionCallback position_callback_;

  // Set if SetOverride() has been called and ClearOverride() has not
  // subsequently been called, `nullptr` otherwise.
  device::mojom::GeopositionResultPtr position_override_;

  device::mojom::GeopositionResultPtr current_position_;

  const GURL url_;

  // Whether this instance is currently observing location updates with high
  // accuracy.
  bool high_accuracy_;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // The binding between this object and the other end of the pipe.
  mojo::Receiver<device::mojom::Geolocation> receiver_;
};

#endif  // CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_GEOLOCATION_BRIDGE_H_
