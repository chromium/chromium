// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/audio/voice_isolation_ui_appearance.h"

#include "base/strings/stringprintf.h"

namespace ash {

VoiceIsolationUIAppearance::VoiceIsolationUIAppearance() = default;

VoiceIsolationUIAppearance::VoiceIsolationUIAppearance(
    cras::AudioEffectType toggle_type,
    uint32_t effect_mode_options,
    bool show_effect_fallback_message)
    : toggle_type(toggle_type),
      effect_mode_options(effect_mode_options),
      show_effect_fallback_message(show_effect_fallback_message) {}

std::string AudioEffectTypesToString(uint32_t types) {
  if (types == 0) {
    return "NONE";
  }

  std::string result;
  if (types & cras::EFFECT_TYPE_NOISE_CANCELLATION) {
    base::StringAppendF(&result, "NOISE_CANCELLATION ");
  }
  if (types & cras::EFFECT_TYPE_HFP_MIC_SR) {
    base::StringAppendF(&result, "HFP_MIC_SR ");
  }
  if (types & cras::EFFECT_TYPE_STYLE_TRANSFER) {
    base::StringAppendF(&result, "STYLE_TRANSFER ");
  }
  if (types & cras::EFFECT_TYPE_BEAMFORMING) {
    base::StringAppendF(&result, "BEAMFORMING ");
  }
  return result;
}

std::string AudioEffectTypeToString(cras::AudioEffectType type) {
  return AudioEffectTypesToString(static_cast<uint32_t>(type));
}

std::string VoiceIsolationUIAppearance::ToString() const {
  std::string result;
  base::StringAppendF(&result, "toggle_type = %s ",
                      AudioEffectTypeToString(toggle_type));
  base::StringAppendF(&result, "effect_mode_options = %s ",
                      AudioEffectTypesToString(effect_mode_options));
  base::StringAppendF(&result, "show_effect_fallback_message = %s ",
                      show_effect_fallback_message ? "true" : "false");
  return result;
}

bool VoiceIsolationUIAppearance::operator==(
    const VoiceIsolationUIAppearance& other) const {
  return toggle_type == other.toggle_type &&
         effect_mode_options == other.effect_mode_options &&
         show_effect_fallback_message == other.show_effect_fallback_message;
}

}  // namespace ash
