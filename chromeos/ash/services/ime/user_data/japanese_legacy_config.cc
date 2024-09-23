// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/user_data/japanese_legacy_config.h"

#include "base/containers/fixed_flat_map.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/fetch_japanese_legacy_config.pb.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"

namespace ash::ime {

using mojom::JapaneseLegacyConfig;

constexpr auto kPreedits =
    base::MakeFixedFlatMap<chromeos_input::PreeditMethod,
                           JapaneseLegacyConfig::PreeditMethod>({
        {chromeos_input::PREEDIT_ROMANJI,
         JapaneseLegacyConfig::PreeditMethod::kRomaji},
        {chromeos_input::PREEDIT_KANA,
         JapaneseLegacyConfig::PreeditMethod::kKana},
    });

constexpr auto kPunctuations =
    base::MakeFixedFlatMap<chromeos_input::PunctuationMethod,
                           JapaneseLegacyConfig::PunctuationMethod>({
        {chromeos_input::PUNCTUATION_KUTEN_TOUTEN,
         JapaneseLegacyConfig::PunctuationMethod::kKutenTouten},
        {chromeos_input::PUNCTUATION_COMMA_PERIOD,
         JapaneseLegacyConfig::PunctuationMethod::kCommaPeriod},
        {chromeos_input::PUNCTUATION_KUTEN_PERIOD,
         JapaneseLegacyConfig::PunctuationMethod::kKutenPeriod},
        {chromeos_input::PUNCTUATION_COMMA_TOUTEN,
         JapaneseLegacyConfig::PunctuationMethod::kCommaTouten},
    });

constexpr auto kSymbols =
    base::MakeFixedFlatMap<chromeos_input::SymbolMethod,
                           JapaneseLegacyConfig::SymbolMethod>({
        {chromeos_input::SYMBOL_CORNER_BRACKET_MIDDLE_DOT,
         JapaneseLegacyConfig::SymbolMethod::kCornerBracketMiddleDot},
        {chromeos_input::SYMBOL_SQUARE_BRACKET_SLASH,
         JapaneseLegacyConfig::SymbolMethod::kSquareBracketSlash},
        {chromeos_input::SYMBOL_CORNER_BRACKET_SLASH,
         JapaneseLegacyConfig::SymbolMethod::kCornerBracketSlash},
        {chromeos_input::SYMBOL_SQUARE_BRACKET_MIDDLE_DOT,
         JapaneseLegacyConfig::SymbolMethod::kSquareBracketMiddleDot},
    });

constexpr auto kFundamentalCharacterForms =
    base::MakeFixedFlatMap<chromeos_input::FundamentalCharacterForm,
                           JapaneseLegacyConfig::FundamentalCharacterForm>({
        {chromeos_input::FUNDAMENTAL_INPUT_MODE,
         JapaneseLegacyConfig::FundamentalCharacterForm::kInputMode},
        {chromeos_input::FUNDAMENTAL_FULL_WIDTH,
         JapaneseLegacyConfig::FundamentalCharacterForm::kFullWidth},
        {chromeos_input::FUNDAMENTAL_HALF_WIDTH,
         JapaneseLegacyConfig::FundamentalCharacterForm::kHalfWidth},
    });

constexpr auto kSelectionShortcuts =
    base::MakeFixedFlatMap<chromeos_input::SelectionShortcut,
                           JapaneseLegacyConfig::SelectionShortcut>({
        {chromeos_input::SELECTION_123456789,
         JapaneseLegacyConfig::SelectionShortcut::k123456789},
        {chromeos_input::SELECTION_ASDFGHJKL,
         JapaneseLegacyConfig::SelectionShortcut::kAsdfghjkl},
        {chromeos_input::SELECTION_NO_SHORTCUT,
         JapaneseLegacyConfig::SelectionShortcut::kNoShortcut},
    });

constexpr auto kSessionKeymaps =
    base::MakeFixedFlatMap<chromeos_input::SessionKeymap,
                           JapaneseLegacyConfig::SessionKeymap>({
        {chromeos_input::KEYMAP_CUSTOM,
         JapaneseLegacyConfig::SessionKeymap::kCustom},
        {chromeos_input::KEYMAP_ATOK,
         JapaneseLegacyConfig::SessionKeymap::kAtok},
        {chromeos_input::KEYMAP_MSIME,
         JapaneseLegacyConfig::SessionKeymap::kMsime},
        {chromeos_input::KEYMAP_KOTOERI,
         JapaneseLegacyConfig::SessionKeymap::kKotoeri},
        {chromeos_input::KEYMAP_MOBILE,
         JapaneseLegacyConfig::SessionKeymap::kMobile},
        {chromeos_input::KEYMAP_CHROMEOS,
         JapaneseLegacyConfig::SessionKeymap::kChromeos},
        {chromeos_input::KEYMAP_NONE,
         JapaneseLegacyConfig::SessionKeymap::kNone},
    });

constexpr auto kShiftKeyModeSwitch =
    base::MakeFixedFlatMap<chromeos_input::ShiftKeyModeSwitch,
                           JapaneseLegacyConfig::ShiftKeyModeSwitch>({
        {chromeos_input::SHIFTKEY_OFF,
         JapaneseLegacyConfig::ShiftKeyModeSwitch::kOff},
        {chromeos_input::SHIFTKEY_ASCII_INPUT_MODE,
         JapaneseLegacyConfig::ShiftKeyModeSwitch::kAsciiInputMode},
        {chromeos_input::SHIFTKEY_KATAKANA,
         JapaneseLegacyConfig::ShiftKeyModeSwitch::kKatakana},
    });

mojom::JapaneseLegacyConfigPtr MakeMojomJapaneseLegacyConfig(
    chromeos_input::FetchJapaneseLegacyConfigResponse proto_response) {
  mojom::JapaneseLegacyConfigPtr response = JapaneseLegacyConfig::New();
  if (proto_response.has_preedit_method()) {
    if (auto it = kPreedits.find(proto_response.preedit_method());
        it != kPreedits.end()) {
      response->preedit_method = it->second;
    }
  }
  if (proto_response.has_punctuation_method()) {
    if (auto it = kPunctuations.find(proto_response.punctuation_method());
        it != kPunctuations.end()) {
      response->punctuation_method = it->second;
    }
  }
  if (proto_response.has_symbol_method()) {
    if (auto it = kSymbols.find(proto_response.symbol_method());
        it != kSymbols.end()) {
      response->symbol_method = it->second;
    }
  }
  if (proto_response.has_space_character_form()) {
    if (auto it = kFundamentalCharacterForms.find(
            proto_response.space_character_form());
        it != kFundamentalCharacterForms.end()) {
      response->space_character_form = it->second;
    }
  }
  if (proto_response.has_selection_shortcut()) {
    if (auto it = kSelectionShortcuts.find(proto_response.selection_shortcut());
        it != kSelectionShortcuts.end()) {
      response->selection_shortcut = it->second;
    }
  }
  if (proto_response.has_session_keymap()) {
    if (auto it = kSessionKeymaps.find(proto_response.session_keymap());
        it != kSessionKeymaps.end()) {
      response->session_keymap = it->second;
    }
  }
  if (proto_response.has_shift_key_mode_switch()) {
    if (auto it =
            kShiftKeyModeSwitch.find(proto_response.shift_key_mode_switch());
        it != kShiftKeyModeSwitch.end()) {
      response->shift_key_mode_switch = it->second;
    }
  }

  if (proto_response.has_incognito_mode()) {
    response->incognito_mode = proto_response.incognito_mode();
  }
  if (proto_response.has_use_auto_conversion()) {
    response->use_auto_conversion = proto_response.use_auto_conversion();
  }
  if (proto_response.has_use_history_suggest()) {
    response->use_history_suggest = proto_response.use_history_suggest();
  }
  if (proto_response.has_use_dictionary_suggest()) {
    response->use_dictionary_suggest = proto_response.use_dictionary_suggest();
  }
  if (proto_response.has_suggestion_size()) {
    response->suggestion_size = proto_response.suggestion_size();
  }
  if (proto_response.has_upload_usage_stats()) {
    response->upload_usage_stats = proto_response.upload_usage_stats();
  }
  return response;
}
}  // namespace ash::ime
