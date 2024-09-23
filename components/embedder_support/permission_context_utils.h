// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_PERMISSION_CONTEXT_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_PERMISSION_CONTEXT_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/contexts/nfc_permission_context.h"
#include "components/permissions/permission_manager.h"
#include "services/device/public/cpp/geolocation/buildflags.h"

namespace content {
class BrowserContext;
}  // namespace content

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
namespace device {
class GeolocationSystemPermissionManager;
}  // namespace device
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

namespace webrtc {
class MediaStreamDeviceEnumerator;
}  // namespace webrtc

namespace embedder_support {

// Contains all delegates & helper classes needed to construct all default
// permission contexts via CreateDefaultPermissionContexts().
struct PermissionContextDelegates {
  PermissionContextDelegates();
  PermissionContextDelegates(PermissionContextDelegates&&);
  PermissionContextDelegates& operator=(PermissionContextDelegates&&);
  ~PermissionContextDelegates();

  std::unique_ptr<permissions::CameraPanTiltZoomPermissionContext::Delegate>
      camera_pan_tilt_zoom_permission_context_delegate;
  std::unique_ptr<permissions::GeolocationPermissionContext::Delegate>
      geolocation_permission_context_delegate;
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  raw_ptr<device::GeolocationSystemPermissionManager>
      geolocation_system_permission_manager;
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  raw_ptr<webrtc::MediaStreamDeviceEnumerator> media_stream_device_enumerator;
  std::unique_ptr<permissions::NfcPermissionContext::Delegate>
      nfc_permission_context_delegate;
};

// Creates default permission contexts shared between Content embedders.
// Embedders are expected to populate all fields of `delegates` which are then
// being used to create the specific permission contexts. Pass `true` for
// `is_regular_profile` if `browser_context` is a `Profile` and
// `Profile::IsRegularProfile` returns `true`.
permissions::PermissionManager::PermissionContextMap
CreateDefaultPermissionContexts(content::BrowserContext* browser_context,
                                bool is_regular_profile,
                                PermissionContextDelegates delegates);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_PERMISSION_CONTEXT_UTILS_H_
