// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_PREF_NAMES_H_
#define COMPONENTS_SHARING_MESSAGE_PREF_NAMES_H_

#include "build/build_config.h"

namespace prefs {

inline constexpr char kSharingFCMRegistration[] = "sharing.fcm_registration";
inline constexpr char kSharingLocalSharingInfo[] = "sharing.local_sharing_info";

// A flag to enable/disable the Shared Clipboard feature which enables users to
// send text across devices.
inline constexpr char kSharedClipboardEnabled[] =
    "browser.shared_clipboard_enabled";

}  // namespace prefs

#endif  // COMPONENTS_SHARING_MESSAGE_PREF_NAMES_H_
