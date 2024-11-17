// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_VOICE_ISOLATION_UI_APPEARANCE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_VOICE_ISOLATION_UI_APPEARANCE_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "third_party/cros_system_api/dbus/audio/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

struct COMPONENT_EXPORT(DBUS_AUDIO) VoiceIsolationUIAppearance {
  cras::AudioEffectType toggle_type = cras::EFFECT_TYPE_NONE;
  uint32_t effect_mode_options = cras::EFFECT_TYPE_NONE;
  bool show_effect_fallback_message = false;

  VoiceIsolationUIAppearance();
  VoiceIsolationUIAppearance(cras::AudioEffectType toggle_type,
                             uint32_t effect_mode_options,
                             bool show_effect_fallback_message);
  std::string ToString() const;
  bool operator==(const VoiceIsolationUIAppearance& other) const;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_VOICE_ISOLATION_UI_APPEARANCE_H_
