// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/user_data/japanese_legacy_config.h"

#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/fetch_japanese_legacy_config.pb.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::ime {
namespace {
using mojom::JapaneseLegacyConfig;

TEST(JapaneseLegacyConfigTest, TestProtoConversion) {
  chromeos_input::FetchJapaneseLegacyConfigResponse proto;
  proto.set_preedit_method(chromeos_input::PREEDIT_KANA);
  proto.set_punctuation_method(chromeos_input::PUNCTUATION_COMMA_PERIOD);
  proto.set_symbol_method(chromeos_input::SYMBOL_CORNER_BRACKET_MIDDLE_DOT);
  proto.set_space_character_form(chromeos_input::FUNDAMENTAL_FULL_WIDTH);
  proto.set_selection_shortcut(chromeos_input::SELECTION_ASDFGHJKL);
  proto.set_session_keymap(chromeos_input::KEYMAP_ATOK);
  proto.set_shift_key_mode_switch(chromeos_input::SHIFTKEY_ASCII_INPUT_MODE);
  proto.set_incognito_mode(true);
  proto.set_use_auto_conversion(true);
  proto.set_use_history_suggest(true);
  proto.set_use_dictionary_suggest(true);
  proto.set_suggestion_size(8);
  proto.set_upload_usage_stats(true);

  mojom::JapaneseLegacyConfigPtr mojom_config =
      MakeMojomJapaneseLegacyConfig(proto);

  mojom::JapaneseLegacyConfigPtr expected_response = JapaneseLegacyConfig::New(
      /* preedit_method=*/JapaneseLegacyConfig::PreeditMethod::kKana,
      /* punctuation_method=*/
      JapaneseLegacyConfig::PunctuationMethod::kCommaPeriod,
      /* symbol_method=*/
      JapaneseLegacyConfig::SymbolMethod::kCornerBracketMiddleDot,
      /* space_character_form=*/
      JapaneseLegacyConfig::FundamentalCharacterForm::kFullWidth,
      /* selection_shortcut=*/
      JapaneseLegacyConfig::SelectionShortcut::kAsdfghjkl,
      /* session_keymap=*/JapaneseLegacyConfig::SessionKeymap::kAtok,
      /* use_auto_conversion=*/true,
      /* shift_key_mode_switch=*/
      JapaneseLegacyConfig::ShiftKeyModeSwitch::kAsciiInputMode,
      /* use_history_suggest=*/true,
      /* use_dictionary_suggest=*/true,
      /* suggestion_size=*/8,
      /* incognito_mode=*/true,
      /* upload_usage_stats=*/true);
  EXPECT_TRUE(mojom_config.Equals(expected_response));
}

}  // namespace
}  // namespace ash::ime
