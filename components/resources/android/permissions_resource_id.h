// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file maps permissions resource IDs to Android resource IDs.

// Presence of regular include guards is checked by:
// 1. cpplint
// 2. a custom presubmit in src/PRESUBMIT.py
// 3. clang (but it only checks the guard is correct if present)
// Disable the first two with these magic comments:
// NOLINT(build/header_guard)
// no-include-guard-because-multiply-included

// LINK_RESOURCE_ID is used for IDs that come from a .grd file.
#ifndef LINK_RESOURCE_ID
#error "LINK_RESOURCE_ID should be defined before including this file"
#endif
// DECLARE_RESOURCE_ID is used for IDs that don't have .grd entries, and
// are only declared in this file.
#ifndef DECLARE_RESOURCE_ID
#error "DECLARE_RESOURCE_ID should be defined before including this file"
#endif

DECLARE_RESOURCE_ID(IDR_ANDROID_GLOBE, R.drawable.ic_globe_24dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_CLIPBOARD,
                    R.drawable.gm_filled_content_paste_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_FOLDER, R.drawable.ic_folder_blue_24dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_GEOLOCATION,
                    R.drawable.gm_filled_location_on_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_HAND_TRACKING,
                    R.drawable.gm_filled_hand_gesture_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_IDLE_DETECTION,
                    R.drawable.gm_filled_devices_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA,
                    R.drawable.gm_filled_videocam_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MEDIA_STREAM_MIC,
                    R.drawable.gm_filled_mic_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MIDI, R.drawable.gm_filled_piano_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_MULTIPLE_DOWNLOADS,
                    R.drawable.infobar_downloading)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_NFC, R.drawable.gm_filled_nfc_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_NOTIFICATIONS,
                    R.drawable.gm_filled_notifications_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER,
                    R.drawable.infobar_protected_media_identifier)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_PERMISSION_COOKIE,
                    R.drawable.permission_cookie)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_WARNING, R.drawable.infobar_warning)
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_VR_HEADSET,
                    R.drawable.gm_filled_cardboard_24)
DECLARE_RESOURCE_ID(IDR_ANDROID_STORAGE_ACCESS, R.drawable.ic_storage_access_24)
// TODO(crbug.com/40252825): use a dedicated icon.
DECLARE_RESOURCE_ID(IDR_ANDROID_INFOBAR_IDENTITY_PROVIDER,
                    R.drawable.gm_filled_devices_24)
