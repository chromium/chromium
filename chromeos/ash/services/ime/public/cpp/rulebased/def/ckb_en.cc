// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ckb_en.h"

namespace ckb_en {

const char* kId = "ckb_en";
bool kIs102 = false;
const char* kNormal[] = {
    "\u20ac",  // BackQuote
    "\u0661",  // Digit1
    "\u0662",  // Digit2
    "\u0663",  // Digit3
    "\u0664",  // Digit4
    "\u0665",  // Digit5
    "\u0666",  // Digit6
    "\u0667",  // Digit7
    "\u0668",  // Digit8
    "\u0669",  // Digit9
    "\u0660",  // Digit0
    "-",       // Minus
    "=",       // Equal
    "\u0642",  // KeyQ
    "\u0648",  // KeyW
    "\u06d5",  // KeyE
    "\u0631",  // KeyR
    "\u062a",  // KeyT
    "\u06cc",  // KeyY
    "\u0626",  // KeyU
    "\u062d",  // KeyI
    "\u06c6",  // KeyO
    "\u067e",  // KeyP
    "}",       // BracketLeft
    "{",       // BracketRight
    "\\",      // Backslash
    "\u0627",  // KeyA
    "\u0633",  // KeyS
    "\u062f",  // KeyD
    "\u0641",  // KeyF
    "\u06af",  // KeyG
    "\u0647",  // KeyH
    "\u0698",  // KeyJ
    "\u06a9",  // KeyK
    "\u0644",  // KeyL
    "\u061b",  // Semicolon
    "'",       // Quote
    "\u0632",  // KeyZ
    "\u062e",  // KeyX
    "\u062c",  // KeyC
    "\u06a4",  // KeyV
    "\u0628",  // KeyB
    "\u0646",  // KeyN
    "\u0645",  // KeyM
    "\u060c",  // Comma
    ".",       // Period
    "/",       // Slash
    "\u0020",  // Space
};
const char* kShift[] = {
    "~",             // BackQuote
    "!",             // Digit1
    "@",             // Digit2
    "#",             // Digit3
    "$",             // Digit4
    "%",             // Digit5
    "\u00bb",        // Digit6
    "\u00ab",        // Digit7
    "*",             // Digit8
    ")",             // Digit9
    "(",             // Digit0
    "_",             // Minus
    "+",             // Equal
    "`",             // KeyQ
    "\u0648\u0648",  // KeyW
    "\u064a",        // KeyE
    "\u0695",        // KeyR
    "\u0637",        // KeyT
    "\u06ce",        // KeyY
    "\u0621",        // KeyU
    "\u0639",        // KeyI
    "\u0624",        // KeyO
    "\u062b",        // KeyP
    "]",             // BracketLeft
    "[",             // BracketRight
    "|",             // Backslash
    "\u0622",        // KeyA
    "\u0634",        // KeyS
    "\u0630",        // KeyD
    "\u0625",        // KeyF
    "\u063a",        // KeyG
    "\u200c",        // KeyH
    "\u0623",        // KeyJ
    "\u0643",        // KeyK
    "\u06b5",        // KeyL
    ":",             // Semicolon
    "\"",            // Quote
    "\u0636",        // KeyZ
    "\u0635",        // KeyX
    "\u0686",        // KeyC
    "\u0638",        // KeyV
    "\u0649",        // KeyB
    "\u0629",        // KeyN
    "\u0640",        // KeyM
    ">",             // Comma
    "<",             // Period
    "\u061f",        // Slash
    "\u200c",        // Space
};
const char** kKeyMap[8] = {kNormal, kShift, kNormal, kShift,
                           kNormal, kShift, kNormal, kShift};

}  // namespace ckb_en
