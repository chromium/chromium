// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/rules_data.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ar.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ckb_ar.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ckb_en.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/fa.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/km.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/lo.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ne_inscript.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ru_phone_aatseel.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ru_phone_yazhert.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_inscript.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_typewriter.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th_pattajoti.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th_tis.h"

namespace chromeos {
namespace ime {
namespace rulebased {

namespace {

struct RawDataEntry {
  const wchar_t** key_map;
  const uint8_t key_map_count;
  const uint8_t* key_map_index;
  bool is_102_keyboard;

  RawDataEntry(const wchar_t** map,
               uint8_t map_count,
               const uint8_t* map_index,
               bool is_102)
      : key_map(map),
        key_map_count(map_count),
        key_map_index(map_index),
        is_102_keyboard(is_102) {}
};

static const std::map<std::string, RawDataEntry> s_data = {
    {id_ar, RawDataEntry(key_map_ar,
                         base::size(key_map_ar),
                         key_map_index_ar,
                         is_102_ar)},
    {id_ckb_ar, RawDataEntry(key_map_ckb_ar,
                             base::size(key_map_ckb_ar),
                             key_map_index_ckb_ar,
                             is_102_ckb_ar)},
    {id_ckb_en, RawDataEntry(key_map_ckb_en,
                             base::size(key_map_ckb_en),
                             key_map_index_ckb_en,
                             is_102_ckb_en)},
    {id_fa, RawDataEntry(key_map_fa,
                         base::size(key_map_fa),
                         key_map_index_fa,
                         is_102_fa)},
    {id_km, RawDataEntry(key_map_km,
                         base::size(key_map_km),
                         key_map_index_km,
                         is_102_km)},
    {id_lo, RawDataEntry(key_map_lo,
                         base::size(key_map_lo),
                         key_map_index_lo,
                         is_102_lo)},
    {id_ne_inscript, RawDataEntry(key_map_ne_inscript,
                                  base::size(key_map_ne_inscript),
                                  key_map_index_ne_inscript,
                                  is_102_ne_inscript)},
    {id_ru_phone_aatseel, RawDataEntry(key_map_ru_phone_aatseel,
                                       base::size(key_map_ru_phone_aatseel),
                                       key_map_index_ru_phone_aatseel,
                                       is_102_ru_phone_aatseel)},
    {id_ru_phone_yazhert, RawDataEntry(key_map_ru_phone_yazhert,
                                       base::size(key_map_ru_phone_yazhert),
                                       key_map_index_ru_phone_yazhert,
                                       is_102_ru_phone_yazhert)},
    {id_ta_inscript, RawDataEntry(key_map_ta_inscript,
                                  base::size(key_map_ta_inscript),
                                  key_map_index_ta_inscript,
                                  is_102_ta_inscript)},
    {id_ta_typewriter, RawDataEntry(key_map_ta_typewriter,
                                    base::size(key_map_ta_typewriter),
                                    key_map_index_ta_typewriter,
                                    is_102_ta_typewriter)},
    {id_th, RawDataEntry(key_map_th,
                         base::size(key_map_th),
                         key_map_index_th,
                         is_102_th)},
    {id_th_pattajoti, RawDataEntry(key_map_th_pattajoti,
                                   base::size(key_map_th_pattajoti),
                                   key_map_index_th_pattajoti,
                                   is_102_th_pattajoti)},
    {id_th_tis, RawDataEntry(key_map_th_tis,
                             base::size(key_map_th_tis),
                             key_map_index_th_tis,
                             is_102_th_tis)}};

static const char* s_101_keys[] = {
    // Row #1
    "BackQuote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
    "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal",
    // Row #2
    "KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO",
    "KeyP", "BracketLeft", "BracketRight", "Backslash",
    // Row #3
    "KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
    "Semicolon", "Quote",
    // Row #4
    "KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM", "Comma", "Period",
    "Slash",
    // Row #5
    "Space"};

static const char* s_102_keys[] = {
    // Row #1
    "BackQuote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
    "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal",
    // Row #2
    "KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO",
    "KeyP", "BracketLeft", "BracketRight",
    // Row #3
    "KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
    "Semicolon", "Quote", "Backslash",
    // Row #4
    "IntlBackslash", "KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM",
    "Comma", "Period", "Slash",
    // Row #5
    "Space"};

bool ScanBrackets(const wchar_t** pstr,
                  wchar_t bracket_left,
                  wchar_t bracket_right,
                  std::string* str_out) {
  const wchar_t* p = *pstr;
  if (*p != bracket_left || *(p + 1) != bracket_left)
    return false;
  p += 2;
  const wchar_t* from = p;
  while (*p != L'\0') {
    if (*p == bracket_right && *(p + 1) == bracket_right) {
      base::WideToUTF8(from, p - from, str_out);
      *pstr = p + 2;
      return true;
    }
    ++p;
  }
  return false;
}

KeyMap ParseKeyMap(const wchar_t* raw_key_map, bool is_102) {
  const char** std_keys = is_102 ? s_102_keys : s_101_keys;
  KeyMap key_map;
  const wchar_t* p = raw_key_map;
  uint8_t index = 0;
  while (*p != L'\0') {
    std::string str;
    if (!ScanBrackets(&p, L'{', L'}', &str) &&
        !ScanBrackets(&p, L'(', L')', &str)) {
      base::WideToUTF8(p++, 1, &str);
    }

    DCHECK((is_102 && index < 49) || (!is_102 && index < 48));
    key_map[std_keys[index++]] = str;
  }
  return key_map;
}

}  // namespace

KeyMap ParseKeyMapForTesting(const wchar_t* raw_key_map, bool is_102) {
  return ParseKeyMap(raw_key_map, is_102);
}

RulesData::RulesData() = default;
RulesData::~RulesData() = default;

// static
std::unique_ptr<const RulesData> RulesData::GetById(const std::string& id) {
  auto it = s_data.find(id);
  if (it == s_data.end())
    return nullptr;

  const RawDataEntry& entry = it->second;
  std::unique_ptr<RulesData> data = std::make_unique<RulesData>();
  for (uint8_t i = 0; i < entry.key_map_count; ++i) {
    data->key_map_cache_.push_back(
        ParseKeyMap(entry.key_map[i], entry.is_102_keyboard));
  }
  for (uint8_t i = 0; i < base::size(data->key_maps_); ++i) {
    uint8_t index = entry.key_map_index[i];
    DCHECK(index < data->key_map_cache_.size());
    data->key_maps_[i] = &(data->key_map_cache_[index]);
  }
  return data;
}

// static
bool RulesData::IsIdSupported(const std::string& id) {
  return s_data.find(id) != s_data.end();
}

const KeyMap* RulesData::GetKeyMapByModifiers(uint8_t modifiers) const {
  return modifiers < 8 ? key_maps_[modifiers] : nullptr;
}

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos
