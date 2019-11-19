// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_PREF_NAMES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_PREF_NAMES_H_

#include "base/component_export.h"

namespace chromeos {
namespace prefs {

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAudioDevicesMute[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kAudioDevicesVolumePercent[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAudioMute[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAudioOutputAllowed[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAudioVolumePercent[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAudioDevicesState[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kQuirksClientLastServerCheck[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDeviceWiFiFastTransitionEnabled[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kSecondaryGoogleAccountSigninAllowed[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kSamlPasswordModifiedTime[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kSamlPasswordExpirationTime[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kSamlPasswordChangeUrl[];

}  // namespace prefs
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_PREF_NAMES_H_
