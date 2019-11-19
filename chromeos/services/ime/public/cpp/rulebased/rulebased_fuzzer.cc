// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/stl_util.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/us.h"
#include "chromeos/services/ime/public/cpp/rulebased/engine.h"
#include "chromeos/services/ime/public/cpp/rulebased/rulebased_fuzzer.pb.h"
#include "chromeos/services/ime/public/cpp/rulebased/rules_data.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace rulebased = chromeos::ime::rulebased;

namespace {

// Must match chromeos/services/ime/public/cpp/rulebased/rulebased_fuzzer.proto
constexpr const char* kKeyCodes[] = {
    "Backquote",    "Digit1", "Digit2",    "Digit3",        "Digit4",
    "Digit5",       "Digit6", "Digit7",    "Digit8",        "Digit9",
    "Digit0",       "Minus",  "Equal",     "Backspace",     "KeyQ",
    "KeyW",         "KeyE",   "KeyR",      "KeyT",          "KeyY",
    "KeyU",         "KeyI",   "KeyO",      "KeyP",          "BracketLeft",
    "BracketRight", "KeyA",   "KeyS",      "KeyD",          "KeyF",
    "KeyG",         "KeyH",   "KeyJ",      "KeyK",          "KeyL",
    "Semicolon",    "Quote",  "Backslash", "IntlBackslash", "Enter",
    "KeyZ",         "KeyX",   "KeyC",      "KeyV",          "KeyB",
    "KeyN",         "KeyM",   "Comma",     "Period",        "Slash",
    "Space"};

// Must match chromeos/services/ime/public/cpp/rulebased/rulebased_fuzzer.proto
constexpr const char* kEngineIds[] = {
    "ar",
    "bn_phone",
    "ckb_ar",
    "ckb_en",
    "deva_phone",
    "ethi",
    "fa",
    "gu_phone",
    "km",
    "kn_phone",
    "lo",
    "ml_phone",
    "my",
    "my_myansan",
    "ne_inscript",
    "ne_phone",
    "ru_phone_aatseel",
    "ru_phone_yazhert",
    "si",
    "ta_inscript",
    "ta_itrans",
    "ta_phone",
    "ta_tamil99",
    "ta_typewriter",
    "te_phone",
    "th",
    "th_pattajoti",
    "th_tis",
    "us",
    "vi_tcvn",
    "vi_telex",
    "vi_viqr",
    "vi_vni",
};

static rulebased::Engine engines[base::size(kEngineIds)];

uint8_t GetModifierFromKeyEvent(const rulebased_fuzzer::KeyEvent& e) {
  uint8_t modifiers = 0;
  if (e.shift())
    modifiers |= rulebased::MODIFIER_SHIFT;
  if (e.altgr())
    modifiers |= rulebased::MODIFIER_ALTGR;
  if (e.caps_lock())
    modifiers |= rulebased::MODIFIER_CAPSLOCK;
  return modifiers;
}

}  // namespace

DEFINE_PROTO_FUZZER(const rulebased_fuzzer::TestFuzzerInput& msg) {
  auto& engine = engines[msg.engine_id()];
  engine.Activate(kEngineIds[msg.engine_id()]);

  for (const auto& event : msg.key_events()) {
    engine.ProcessKey(kKeyCodes[event.code()], GetModifierFromKeyEvent(event));
  }
}
