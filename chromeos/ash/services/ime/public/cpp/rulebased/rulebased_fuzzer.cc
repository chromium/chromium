// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/us.h"
#include "chromeos/ash/services/ime/public/cpp/rulebased/engine.h"
#include "chromeos/ash/services/ime/public/cpp/rulebased/rulebased_fuzzer.pb.h"
#include "chromeos/ash/services/ime/public/cpp/rulebased/rules_data.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace rulebased = ::ash::ime::rulebased;
namespace mojom = ::ash::ime::mojom;

namespace {

// Must match
// chromeos/ash/services/ime/public/cpp/rulebased/rulebased_fuzzer.proto
constexpr mojom::DomCode kKeyCodes[] = {
    mojom::DomCode::kBackquote,     mojom::DomCode::kDigit1,
    mojom::DomCode::kDigit2,        mojom::DomCode::kDigit3,
    mojom::DomCode::kDigit4,        mojom::DomCode::kDigit5,
    mojom::DomCode::kDigit6,        mojom::DomCode::kDigit7,
    mojom::DomCode::kDigit8,        mojom::DomCode::kDigit9,
    mojom::DomCode::kDigit0,        mojom::DomCode::kMinus,
    mojom::DomCode::kEqual,         mojom::DomCode::kBackspace,
    mojom::DomCode::kKeyQ,          mojom::DomCode::kKeyW,
    mojom::DomCode::kKeyE,          mojom::DomCode::kKeyR,
    mojom::DomCode::kKeyT,          mojom::DomCode::kKeyY,
    mojom::DomCode::kKeyU,          mojom::DomCode::kKeyI,
    mojom::DomCode::kKeyO,          mojom::DomCode::kKeyP,
    mojom::DomCode::kBracketLeft,   mojom::DomCode::kBracketRight,
    mojom::DomCode::kKeyA,          mojom::DomCode::kKeyS,
    mojom::DomCode::kKeyD,          mojom::DomCode::kKeyF,
    mojom::DomCode::kKeyG,          mojom::DomCode::kKeyH,
    mojom::DomCode::kKeyJ,          mojom::DomCode::kKeyK,
    mojom::DomCode::kKeyL,          mojom::DomCode::kSemicolon,
    mojom::DomCode::kQuote,         mojom::DomCode::kBackslash,
    mojom::DomCode::kIntlBackslash, mojom::DomCode::kEnter,
    mojom::DomCode::kKeyZ,          mojom::DomCode::kKeyX,
    mojom::DomCode::kKeyC,          mojom::DomCode::kKeyV,
    mojom::DomCode::kKeyB,          mojom::DomCode::kKeyN,
    mojom::DomCode::kKeyM,          mojom::DomCode::kComma,
    mojom::DomCode::kPeriod,        mojom::DomCode::kSlash,
    mojom::DomCode::kSpace};

// Must match
// chromeos/ash/services/ime/public/cpp/rulebased/rulebased_fuzzer.proto
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

static rulebased::Engine engines[std::size(kEngineIds)];

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
