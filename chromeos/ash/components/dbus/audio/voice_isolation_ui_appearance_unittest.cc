// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/audio/voice_isolation_ui_appearance.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/audio/dbus-constants.h"

namespace ash {
namespace {

class VoiceIsolationUIAppearanceTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(VoiceIsolationUIAppearanceTest, Equal) {
  VoiceIsolationUIAppearance a1(
      cras::AudioEffectType::EFFECT_TYPE_STYLE_TRANSFER,
      cras::AudioEffectType::EFFECT_TYPE_STYLE_TRANSFER |
          cras::AudioEffectType::EFFECT_TYPE_BEAMFORMING,
      true);
  VoiceIsolationUIAppearance a2 = a1;
  EXPECT_EQ(a1, a2);
  a2.toggle_type = cras::AudioEffectType::EFFECT_TYPE_NOISE_CANCELLATION;
  EXPECT_NE(a1, a2);
}

TEST_F(VoiceIsolationUIAppearanceTest, ToString) {
  VoiceIsolationUIAppearance appearance(
      cras::AudioEffectType::EFFECT_TYPE_STYLE_TRANSFER,
      cras::AudioEffectType::EFFECT_TYPE_STYLE_TRANSFER |
          cras::AudioEffectType::EFFECT_TYPE_BEAMFORMING,
      true);
  std::string str = appearance.ToString();
  size_t toggle_pos = str.find("toggle_type");
  size_t effect_mode_options_pos = str.find("effect_mode_options");
  size_t show_effect_fallback_message_pos =
      str.find("show_effect_fallback_message");
  EXPECT_NE(toggle_pos, std::string::npos);
  EXPECT_NE(effect_mode_options_pos, std::string::npos);
  EXPECT_NE(show_effect_fallback_message_pos, std::string::npos);

  std::string toggle_type_str =
      str.substr(toggle_pos, effect_mode_options_pos - toggle_pos);
  EXPECT_NE(toggle_type_str.find("STYLE_TRANSFER"), std::string::npos);

  std::string effect_mode_options_str =
      str.substr(effect_mode_options_pos,
                 show_effect_fallback_message_pos - effect_mode_options_pos);
  EXPECT_NE(effect_mode_options_str.find("STYLE_TRANSFER"), std::string::npos);
  EXPECT_NE(effect_mode_options_str.find("BEAMFORMING"), std::string::npos);

  std::string show_effect_fallback_message_str =
      str.substr(show_effect_fallback_message_pos,
                 str.length() - show_effect_fallback_message_pos);
  EXPECT_NE(str.find("toggle_type"), std::string::npos);
}

}  // namespace
}  // namespace ash
