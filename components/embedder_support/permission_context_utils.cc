// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/permission_context_utils.h"

#include "build/build_config.h"
#include "components/background_sync/background_sync_permission_context.h"
#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"
#include "components/permissions/contexts/clipboard_read_write_permission_context.h"
#include "components/permissions/contexts/clipboard_sanitized_write_permission_context.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/contexts/keyboard_lock_permission_context.h"
#include "components/permissions/contexts/midi_permission_context.h"
#include "components/permissions/contexts/midi_sysex_permission_context.h"
#include "components/permissions/contexts/nfc_permission_context.h"
#include "components/permissions/contexts/payment_handler_permission_context.h"
#include "components/permissions/contexts/pointer_lock_permission_context.h"
#include "components/permissions/contexts/sensor_permission_context.h"
#include "components/permissions/contexts/wake_lock_permission_context.h"
#include "components/permissions/contexts/webxr_permission_context.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/permissions/contexts/nfc_permission_context_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "components/permissions/contexts/geolocation_permission_context_system.h"
#include "services/device/public/cpp/device_features.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
#endif

namespace embedder_support {

PermissionContextDelegates::PermissionContextDelegates() = default;

PermissionContextDelegates::PermissionContextDelegates(
    PermissionContextDelegates&&) = default;

PermissionContextDelegates& PermissionContextDelegates::operator=(
    PermissionContextDelegates&&) = default;

PermissionContextDelegates::~PermissionContextDelegates() = default;

permissions::PermissionManager::PermissionContextMap
CreateDefaultPermissionContexts(content::BrowserContext* browser_context,
                                bool is_regular_profile,
                                PermissionContextDelegates delegates) {
  permissions::PermissionManager::PermissionContextMap permission_contexts;

  DCHECK(delegates.camera_pan_tilt_zoom_permission_context_delegate);
  DCHECK(delegates.geolocation_permission_context_delegate);
  DCHECK(delegates.media_stream_device_enumerator);
  DCHECK(delegates.nfc_permission_context_delegate);

  permission_contexts[ContentSettingsType::AR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          browser_context, ContentSettingsType::AR);
  permission_contexts[ContentSettingsType::BACKGROUND_SYNC] =
      std::make_unique<BackgroundSyncPermissionContext>(browser_context);
  permission_contexts[ContentSettingsType::CAMERA_PAN_TILT_ZOOM] =
      std::make_unique<permissions::CameraPanTiltZoomPermissionContext>(
          browser_context,
          std::move(delegates.camera_pan_tilt_zoom_permission_context_delegate),
          delegates.media_stream_device_enumerator);
  permission_contexts[ContentSettingsType::CLIPBOARD_READ_WRITE] =
      std::make_unique<permissions::ClipboardReadWritePermissionContext>(
          browser_context);
  permission_contexts[ContentSettingsType::CLIPBOARD_SANITIZED_WRITE] =
      std::make_unique<permissions::ClipboardSanitizedWritePermissionContext>(
          browser_context);
#if BUILDFLAG(IS_ANDROID)
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContextAndroid>(
          browser_context,
          std::move(delegates.geolocation_permission_context_delegate),
          is_regular_profile);
#elif BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  if (features::IsOsLevelGeolocationPermissionSupportEnabled()) {
    permission_contexts[ContentSettingsType::GEOLOCATION] =
        std::make_unique<permissions::GeolocationPermissionContextSystem>(
            browser_context,
            std::move(delegates.geolocation_permission_context_delegate));
  } else {
    permission_contexts[ContentSettingsType::GEOLOCATION] =
        std::make_unique<permissions::GeolocationPermissionContext>(
            browser_context,
            std::move(delegates.geolocation_permission_context_delegate));
  }
#else
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContext>(
          browser_context,
          std::move(delegates.geolocation_permission_context_delegate));
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_VR)
  if (device::features::IsHandTrackingEnabled()) {
    permission_contexts[ContentSettingsType::HAND_TRACKING] =
        std::make_unique<permissions::WebXrPermissionContext>(
            browser_context, ContentSettingsType::HAND_TRACKING);
  }
#endif
  permission_contexts[ContentSettingsType::KEYBOARD_LOCK] =
      std::make_unique<permissions::KeyboardLockPermissionContext>(
          browser_context);
  permission_contexts[ContentSettingsType::MIDI] =
      std::make_unique<permissions::MidiPermissionContext>(browser_context);
  permission_contexts[ContentSettingsType::MIDI_SYSEX] =
      std::make_unique<permissions::MidiSysexPermissionContext>(
          browser_context);
#if BUILDFLAG(IS_ANDROID)
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContextAndroid>(
          browser_context,
          std::move(delegates.nfc_permission_context_delegate));
#else
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContext>(
          browser_context,
          std::move(delegates.nfc_permission_context_delegate));
#endif  // BUILDFLAG(IS_ANDROID)
  permission_contexts[ContentSettingsType::PAYMENT_HANDLER] =
      std::make_unique<payments::PaymentHandlerPermissionContext>(
          browser_context);
  permission_contexts[ContentSettingsType::POINTER_LOCK] =
      std::make_unique<permissions::PointerLockPermissionContext>(
          browser_context);
  permission_contexts[ContentSettingsType::SENSORS] =
      std::make_unique<permissions::SensorPermissionContext>(browser_context);
  permission_contexts[ContentSettingsType::VR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          browser_context, ContentSettingsType::VR);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SCREEN] =
      std::make_unique<permissions::WakeLockPermissionContext>(
          browser_context, ContentSettingsType::WAKE_LOCK_SCREEN);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SYSTEM] =
      std::make_unique<permissions::WakeLockPermissionContext>(
          browser_context, ContentSettingsType::WAKE_LOCK_SYSTEM);

  return permission_contexts;
}

}  // namespace embedder_support
