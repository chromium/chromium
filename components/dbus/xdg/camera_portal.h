// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_CAMERA_PORTAL_H_
#define COMPONENTS_DBUS_XDG_CAMERA_PORTAL_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/xdg/request.h"

namespace dbus {
class Bus;
}

namespace dbus_xdg {

enum class CameraPortalError {
  // The user denied access to the cameras.
  kDenied,
  // The portal is missing or failed, or the connection couldn't be registered.
  kFailed,
};

using CameraPortalResult = base::expected<base::ScopedFD, CameraPortalError>;
using CameraPortalCallback = base::OnceCallback<void(CameraPortalResult)>;

// Opens a PipeWire remote for the cameras through the XDG Camera portal.
//
// Unlike the other portals, this one uses a private D-Bus connection that is
// registered with the desktop file name of the browser (e.g. "google-chrome")
// instead of the application ID (e.g. "com.google.Chrome"). The portal shows
// the camera dialog through the desktop environment, which may only allow
// dialogs for the focused app. GNOME Shell maps the browser's windows to the
// desktop file named after their app ID, so the portal app ID must match it.
//
// Not supported in Flatpak or Snap, where the sandbox sets the app ID.
class COMPONENT_EXPORT(COMPONENTS_DBUS) CameraPortal {
 public:
  // Creates a private session bus connection.
  CameraPortal();
  // Uses `bus`, which must not have made any portal calls yet. For testing.
  explicit CameraPortal(scoped_refptr<dbus::Bus> bus);

  CameraPortal(const CameraPortal&) = delete;
  CameraPortal& operator=(const CameraPortal&) = delete;

  // Shuts down the bus. The callback doesn't run after this.
  ~CameraPortal();

  // Registers the connection, requests camera access and opens the PipeWire
  // remote. Runs `callback` with the PipeWire fd on success. May only be called
  // once. `this` may be destroyed by `callback`.
  void OpenPipeWireRemote(CameraPortalCallback callback);

 private:
  void OnRegisterResponse(dbus_utils::CallMethodResult<> result);
  void OnAccessCameraResponse(Results results);
  void OnOpenPipeWireRemoteResponse(
      dbus_utils::CallMethodResult<base::ScopedFD> result);
  void Finish(CameraPortalResult result);

  scoped_refptr<dbus::Bus> bus_;
  CameraPortalCallback callback_;
  std::unique_ptr<Request> access_camera_request_;
  base::WeakPtrFactory<CameraPortal> weak_ptr_factory_{this};
};

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_CAMERA_PORTAL_H_
