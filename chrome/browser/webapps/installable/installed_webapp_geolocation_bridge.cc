// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/installable/installed_webapp_geolocation_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/webapps/installable/installed_webapp_geolocation_context.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/InstalledWebappGeolocationBridge_jni.h"

InstalledWebappGeolocationBridge::InstalledWebappGeolocationBridge(
    mojo::PendingReceiver<Geolocation> receiver,
    const GURL& url,
    InstalledWebappGeolocationContext* context)
    : context_(context),
      url_(url),
      high_accuracy_(false),
      receiver_(this, std::move(receiver)) {
  DCHECK(context_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&InstalledWebappGeolocationBridge::OnConnectionError,
                     base::Unretained(this)));
}

InstalledWebappGeolocationBridge::~InstalledWebappGeolocationBridge() {
  StopUpdates();
}

void InstalledWebappGeolocationBridge::StartListeningForUpdates() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (java_ref_.is_null()) {
    java_ref_.Reset(Java_InstalledWebappGeolocationBridge_create(
        env, reinterpret_cast<intptr_t>(this),
        url::GURLAndroid::FromNativeGURL(env, url_)));
  }
  Java_InstalledWebappGeolocationBridge_start(env, java_ref_, high_accuracy_);
}

void InstalledWebappGeolocationBridge::StopUpdates() {
  if (!java_ref_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InstalledWebappGeolocationBridge_stopAndDestroy(env, java_ref_);
    java_ref_.Reset();
  }
}

void InstalledWebappGeolocationBridge::SetHighAccuracy(bool high_accuracy) {
  high_accuracy_ = high_accuracy;

  if (position_override_ && position_override_->is_position() &&
      device::ValidateGeoposition(*position_override_->get_position())) {
    OnLocationUpdate(position_override_.Clone());
    return;
  }

  StartListeningForUpdates();
}

void InstalledWebappGeolocationBridge::QueryNextPosition(
    QueryNextPositionCallback callback) {
  if (!position_callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextPosition!";
    OnConnectionError();  // Simulate a connection error.
    return;
  }

  position_callback_ = std::move(callback);

  if (current_position_) {
    ReportCurrentPosition();
  }
}

void InstalledWebappGeolocationBridge::SetOverride(
    device::mojom::GeopositionResultPtr result) {
  CHECK(result);
  if (current_position_ && !position_callback_.is_null()) {
    ReportCurrentPosition();
  }

  position_override_ = std::move(result);
  StopUpdates();

  OnLocationUpdate(position_override_.Clone());
}

void InstalledWebappGeolocationBridge::ClearOverride() {
  position_override_.reset();
  StartListeningForUpdates();
}

void InstalledWebappGeolocationBridge::OnPermissionRevoked() {
  if (!position_callback_.is_null()) {
    std::move(position_callback_)
        .Run(device::mojom::GeopositionResult::NewError(
            device::mojom::GeopositionError::New(
                device::mojom::GeopositionErrorCode::kPermissionDenied,
                /*error_message=*/"User denied Geolocation",
                /*error_technical=*/"")));
  }
  position_callback_.Reset();
}

void InstalledWebappGeolocationBridge::OnConnectionError() {
  context_->OnConnectionError(this);

  // The above call deleted this instance, so the only safe thing to do is
  // return.
}

void InstalledWebappGeolocationBridge::OnLocationUpdate(
    device::mojom::GeopositionResultPtr result) {
  DCHECK(context_);
  CHECK(result);

  current_position_ = std::move(result);

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void InstalledWebappGeolocationBridge::ReportCurrentPosition() {
  DCHECK(position_callback_);
  CHECK(current_position_);
  std::move(position_callback_).Run(std::move(current_position_));
}

void InstalledWebappGeolocationBridge::OnNewLocationAvailable(
    JNIEnv* env,
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
    jdouble speed) {
  auto position = device::mojom::Geoposition::New();
  position->latitude = latitude;
  position->longitude = longitude;
  position->timestamp = base::Time::FromSecondsSinceUnixEpoch(time_stamp);
  if (has_altitude)
    position->altitude = altitude;
  if (has_accuracy)
    position->accuracy = accuracy;
  if (has_heading)
    position->heading = heading;
  if (has_speed)
    position->speed = speed;

  // If position is invalid, mark it as unavailable.
  device::mojom::GeopositionResultPtr result;
  if (device::ValidateGeoposition(*position)) {
    result = device::mojom::GeopositionResult::NewPosition(std::move(position));
  } else {
    result = device::mojom::GeopositionResult::NewError(
        device::mojom::GeopositionError::New(
            device::mojom::GeopositionErrorCode::kPositionUnavailable,
            /*error_message=*/"", /*error_technical=*/""));
  }

  OnLocationUpdate(std::move(result));
}

void InstalledWebappGeolocationBridge::OnNewErrorAvailable(
    JNIEnv* env,
    std::string& message) {
  OnLocationUpdate(device::mojom::GeopositionResult::NewError(
      device::mojom::GeopositionError::New(
          device::mojom::GeopositionErrorCode::kPositionUnavailable, message,
          /*error_technical=*/"")));
}
