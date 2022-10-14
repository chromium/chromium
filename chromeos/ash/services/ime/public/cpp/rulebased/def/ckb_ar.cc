// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ckb_ar.h"

namespace ckb_ar {

const char* kId = "ckb_ar";
bool kIs102 = false;
const char* kNormal[] = {
    "\u0698",        // BackQuote
    "\u0661",        // Digit1
    "\u0662",        // Digit2
    "\u0663",        // Digit3
    "\u0664",        // Digit4
    "\u0665",        // Digit5
    "\u0666",        // Digit6
    "\u0667",        // Digit7
    "\u0668",        // Digit8
    "\u0669",        // Digit9
    "\u0660",        // Digit0
    "-",             // Minus
    "=",             // Equal
    "\u0686",        // KeyQ
    "\u0635",        // KeyW
    "\u067e",        // KeyE
    "\u0642",        // KeyR
    "\u0641",        // KeyT
    "\u063a",        // KeyY
    "\u0639",        // KeyU
    "\u0647",        // KeyI
    "\u062e",        // KeyO
    "\u062d",        // KeyP
    "\u062c",        // BracketLeft
    "\u062f",        // BracketRight
    "\\",            // Backslash
    "\u0634",        // KeyA
    "\u0633",        // KeyS
    "\u06cc",        // KeyD
    "\u0628",        // KeyF
    "\u0644",        // KeyG
    "\u0627",        // KeyH
    "\u062a",        // KeyJ
    "\u0646",        // KeyK
    "\u0645",        // KeyL
    "\u06a9",        // Semicolon
    "\u06af",        // Quote
    "\u0626",        // KeyZ
    "\u0621",        // KeyX
    "\u06c6",        // KeyC
    "\u0631",        // KeyV
    "\u0644\u0627",  // KeyB
    "\u0649",        // KeyN
    "\u0647\u200c",  // KeyM
    "\u0648",        // Comma
    "\u0632",        // Period
    "/",             // Slash
    "\u0020",        // Space
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
    "\u0636",        // KeyQ
    "}",             // KeyW
    "\u062b",        // KeyE
    "{",             // KeyR
    "\u06a4",        // KeyT
    "\u0625",        // KeyY
    "",              // KeyU
    "'",             // KeyI
    "\"",            // KeyO
    "\u061b",        // KeyP
    ">",             // BracketLeft
    "<",             // BracketRight
    "|",             // Backslash
    "]",             // KeyA
    "[",             // KeyS
    "\u06ce",        // KeyD
    "",              // KeyF
    "\u06b5",        // KeyG
    "\u0623",        // KeyH
    "\u0640",        // KeyJ
    "\u060c",        // KeyK
    "/",             // KeyL
    ":",             // Semicolon
    "\u0637",        // Quote
    "\u2904",        // KeyZ
    "\u0648\u0648",  // KeyX
    "\u0624",        // KeyC
    "\u0695",        // KeyV
    "\u06b5\u0627",  // KeyB
    "\u0622",        // KeyN
    "\u0629",        // KeyM
    ",",             // Comma
    ".",             // Period
    "\u061f",        // Slash
    "\u200c",        // Space
};
const char** kKeyMap[8] = {kNormal, kShift, kNormal, kShift,
                           kNormal, kShift, kNormal, kShift};

}  // namespace ckb_ar
