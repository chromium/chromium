// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/si.h"

#include <iterator>

namespace si {

const char* kId = "si";
bool kIs102 = false;
const char* kNormal[] = {
    "\u0dca\u200d\u0dbb",  // BackQuote
    "1",                   // Digit1
    "2",                   // Digit2
    "3",                   // Digit3
    "4",                   // Digit4
    "5",                   // Digit5
    "6",                   // Digit6
    "7",                   // Digit7
    "8",                   // Digit8
    "9",                   // Digit9
    "0",                   // Digit0
    "-",                   // Minus
    "=",                   // Equal
    "\u0dd4",              // KeyQ
    "\u0d85",              // KeyW
    "\u0dd0",              // KeyE
    "\u0dbb",              // KeyR
    "\u0d91",              // KeyT
    "\u0dc4",              // KeyY
    "\u0db8",              // KeyU
    "\u0dc3",              // KeyI
    "\u0daf",              // KeyO
    "\u0da0",              // KeyP
    "\u0da4",              // BracketLeft
    ";",                   // BracketRight
    "\u200d\u0dca",        // Backslash
    "\u0dca",              // KeyA
    "\u0dd2",              // KeyS
    "\u0dcf",              // KeyD
    "\u0dd9",              // KeyF
    "\u0da7",              // KeyG
    "\u0dba",              // KeyH
    "\u0dc0",              // KeyJ
    "\u0db1",              // KeyK
    "\u0d9a",              // KeyL
    "\u0dad",              // Semicolon
    ".",                   // Quote
    "'",                   // KeyZ
    "\u0d82",              // KeyX
    "\u0da2",              // KeyC
    "\u0da9",              // KeyV
    "\u0d89",              // KeyB
    "\u0db6",              // KeyN
    "\u0db4",              // KeyM
    "\u0dbd",              // Comma
    "\u0d9c",              // Period
    "?",                   // Slash
    "\u0020",              // Space
};
const char* kShift[] = {
    "\u0dbb\u0dca\u200d",  // BackQuote
    "!",                   // Digit1
    "@",                   // Digit2
    "#",                   // Digit3
    "$",                   // Digit4
    "%",                   // Digit5
    "^",                   // Digit6
    "&",                   // Digit7
    "*",                   // Digit8
    "(",                   // Digit9
    ")",                   // Digit0
    "_",                   // Minus
    "+",                   // Equal
    "\u0dd6",              // KeyQ
    "\u0d8b",              // KeyW
    "\u0dd1",              // KeyE
    "\u0d8d",              // KeyR
    "\u0d94",              // KeyT
    "\u0dc1",              // KeyY
    "\u0db9",              // KeyU
    "\u0dc2",              // KeyI
    "\u0db0",              // KeyO
    "\u0da1",              // KeyP
    "\u0da5",              // BracketLeft
    ":",                   // BracketRight
    "\u0dca\u200d",        // Backslash
    "\u0ddf",              // KeyA
    "\u0dd3",              // KeyS
    "\u0dd8",              // KeyD
    "\u0dc6",              // KeyF
    "\u0da8",              // KeyG
    "\u0dca\u200d\u0dba",  // KeyH
    "",                    // KeyJ
    "\u0dab",              // KeyK
    "\u0d9b",              // KeyL
    "\u0dae",              // Semicolon
    ",",                   // Quote
    "\"",                  // KeyZ
    "\u0d9e",              // KeyX
    "\u0da3",              // KeyC
    "\u0daa",              // KeyV
    "\u0d8a",              // KeyB
    "\u0db7",              // KeyN
    "\u0db5",              // KeyM
    "\u0dc5",              // Comma
    "\u0d9d",              // Period
    "/",                   // Slash
    "\u0020",              // Space
};
const char* kAltGr[] = {
    "",        // BackQuote
    "",        // Digit1
    "",        // Digit2
    "",        // Digit3
    "",        // Digit4
    "",        // Digit5
    "",        // Digit6
    "",        // Digit7
    "",        // Digit8
    "",        // Digit9
    "",        // Digit0
    "",        // Minus
    "",        // Equal
    "",        // KeyQ
    "",        // KeyW
    "",        // KeyE
    "",        // KeyR
    "",        // KeyT
    "",        // KeyY
    "",        // KeyU
    "",        // KeyI
    "\u0db3",  // KeyO
    "",        // KeyP
    "",        // BracketLeft
    "",        // BracketRight
    "",        // Backslash
    "\u0df3",  // KeyA
    "",        // KeyS
    "",        // KeyD
    "",        // KeyF
    "",        // KeyG
    "",        // KeyH
    "",        // KeyJ
    "",        // KeyK
    "",        // KeyL
    "",        // Semicolon
    "\u0df4",  // Quote
    "\u0d80",  // KeyZ
    "\u0d83",  // KeyX
    "\u0da6",  // KeyC
    "\u0dac",  // KeyV
    "",        // KeyB
    "",        // KeyN
    "",        // KeyM
    "\u0d8f",  // Comma
    "\u0d9f",  // Period
    "",        // Slash
    "\u0020",  // Space
};
const char* kCapslock[] = {
    "\u0dbb\u0dca\u200d",  // BackQuote
    "!",                   // Digit1
    "@",                   // Digit2
    "#",                   // Digit3
    "$",                   // Digit4
    "%",                   // Digit5
    "^",                   // Digit6
    "&",                   // Digit7
    "*",                   // Digit8
    "(",                   // Digit9
    ")",                   // Digit0
    "_",                   // Minus
    "+",                   // Equal
    "\u0dd6",              // KeyQ
    "\u0d8b",              // KeyW
    "\u0dd1",              // KeyE
    "\u0d8d",              // KeyR
    "\u0d94",              // KeyT
    "\u0dc1",              // KeyY
    "\u0db9",              // KeyU
    "\u0dc2",              // KeyI
    "\u0db0",              // KeyO
    "\u0da1",              // KeyP
    "\u0da5",              // BracketLeft
    ":",                   // BracketRight
    "\u0dca\u200d",        // Backslash
    "\u0ddf",              // KeyA
    "\u0dd3",              // KeyS
    "\u0dd8",              // KeyD
    "\u0dc6",              // KeyF
    "\u0da8",              // KeyG
    "\u0dca\u200d\u0dba",  // KeyH
    "",                    // KeyJ
    "\u0dab",              // KeyK
    "\u0d9b",              // KeyL
    "\u0dae",              // Semicolon
    ",",                   // Quote
    "\"",                  // KeyZ
    "\u0d9e",              // KeyX
    "\u0da3",              // KeyC
    "\u0daa",              // KeyV
    "\u0d8a",              // KeyB
    "\u0db7",              // KeyN
    "\u0db5",              // KeyM
    "\u0dc5",              // Comma
    "\u0d9d",              // Period
    "/",                   // Slash
    "\u0020",              // Space
};
const char* kShiftAltGr[] = {
    "",        // BackQuote
    "",        // Digit1
    "",        // Digit2
    "",        // Digit3
    "",        // Digit4
    "",        // Digit5
    "",        // Digit6
    "",        // Digit7
    "",        // Digit8
    "",        // Digit9
    "",        // Digit0
    "",        // Minus
    "",        // Equal
    "",        // KeyQ
    "",        // KeyW
    "",        // KeyE
    "",        // KeyR
    "",        // KeyT
    "",        // KeyY
    "",        // KeyU
    "",        // KeyI
    "\u0db3",  // KeyO
    "",        // KeyP
    "",        // BracketLeft
    "",        // BracketRight
    "",        // Backslash
    "\u0df3",  // KeyA
    "",        // KeyS
    "",        // KeyD
    "",        // KeyF
    "",        // KeyG
    "",        // KeyH
    "",        // KeyJ
    "",        // KeyK
    "",        // KeyL
    "",        // Semicolon
    "\u0df4",  // Quote
    "\u0d80",  // KeyZ
    "\u0d83",  // KeyX
    "\u0da6",  // KeyC
    "\u0dac",  // KeyV
    "",        // KeyB
    "",        // KeyN
    "",        // KeyM
    "\u0d8f",  // Comma
    "\u0d9f",  // Period
    "",        // Slash
    "\u0020",  // Space
};
const char* kAltgrCapslock[] = {
    "",        // BackQuote
    "",        // Digit1
    "",        // Digit2
    "",        // Digit3
    "",        // Digit4
    "",        // Digit5
    "",        // Digit6
    "",        // Digit7
    "",        // Digit8
    "",        // Digit9
    "",        // Digit0
    "",        // Minus
    "",        // Equal
    "",        // KeyQ
    "",        // KeyW
    "",        // KeyE
    "",        // KeyR
    "",        // KeyT
    "",        // KeyY
    "",        // KeyU
    "",        // KeyI
    "\u0db3",  // KeyO
    "",        // KeyP
    "",        // BracketLeft
    "",        // BracketRight
    "",        // Backslash
    "\u0df3",  // KeyA
    "",        // KeyS
    "",        // KeyD
    "",        // KeyF
    "",        // KeyG
    "",        // KeyH
    "",        // KeyJ
    "",        // KeyK
    "",        // KeyL
    "",        // Semicolon
    "\u0df4",  // Quote
    "\u0d80",  // KeyZ
    "\u0d83",  // KeyX
    "\u0da6",  // KeyC
    "\u0dac",  // KeyV
    "",        // KeyB
    "",        // KeyN
    "",        // KeyM
    "\u0d8f",  // Comma
    "\u0d9f",  // Period
    "",        // Slash
    "\u0020",  // Space
};
const char* kShiftCapslock[] = {
    "\u0dca\u200d\u0dbb",  // BackQuote
    "1",                   // Digit1
    "2",                   // Digit2
    "3",                   // Digit3
    "4",                   // Digit4
    "5",                   // Digit5
    "6",                   // Digit6
    "7",                   // Digit7
    "8",                   // Digit8
    "9",                   // Digit9
    "0",                   // Digit0
    "-",                   // Minus
    "=",                   // Equal
    "\u0dd4",              // KeyQ
    "\u0d85",              // KeyW
    "\u0dd0",              // KeyE
    "\u0dbb",              // KeyR
    "\u0d91",              // KeyT
    "\u0dc4",              // KeyY
    "\u0db8",              // KeyU
    "\u0dc3",              // KeyI
    "\u0daf",              // KeyO
    "\u0da0",              // KeyP
    "\u0da4",              // BracketLeft
    ";",                   // BracketRight
    "\u200d\u0dca",        // Backslash
    "\u0dca",              // KeyA
    "\u0dd2",              // KeyS
    "\u0dcf",              // KeyD
    "\u0dd9",              // KeyF
    "\u0da7",              // KeyG
    "\u0dba",              // KeyH
    "\u0dc0",              // KeyJ
    "\u0db1",              // KeyK
    "\u0d9a",              // KeyL
    "\u0dad",              // Semicolon
    ".",                   // Quote
    "'",                   // KeyZ
    "\u0d82",              // KeyX
    "\u0da2",              // KeyC
    "\u0da9",              // KeyV
    "\u0d89",              // KeyB
    "\u0db6",              // KeyN
    "\u0db4",              // KeyM
    "\u0dbd",              // Comma
    "\u0d9c",              // Period
    "?",                   // Slash
    "\u0020",              // Space
};
const char* kShiftAltGrCapslock[] = {
    "",        // BackQuote
    "",        // Digit1
    "",        // Digit2
    "",        // Digit3
    "",        // Digit4
    "",        // Digit5
    "",        // Digit6
    "",        // Digit7
    "",        // Digit8
    "",        // Digit9
    "",        // Digit0
    "",        // Minus
    "",        // Equal
    "",        // KeyQ
    "",        // KeyW
    "",        // KeyE
    "",        // KeyR
    "",        // KeyT
    "",        // KeyY
    "",        // KeyU
    "",        // KeyI
    "\u0db3",  // KeyO
    "",        // KeyP
    "",        // BracketLeft
    "",        // BracketRight
    "",        // Backslash
    "\u0df3",  // KeyA
    "",        // KeyS
    "",        // KeyD
    "",        // KeyF
    "",        // KeyG
    "",        // KeyH
    "",        // KeyJ
    "",        // KeyK
    "",        // KeyL
    "",        // Semicolon
    "\u0df4",  // Quote
    "\u0d80",  // KeyZ
    "\u0d83",  // KeyX
    "\u0da6",  // KeyC
    "\u0dac",  // KeyV
    "",        // KeyB
    "",        // KeyN
    "",        // KeyM
    "\u0d8f",  // Comma
    "\u0d9f",  // Period
    "",        // Slash
    "\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};
const char* kTransforms[] = {"\u0d85\u0dcf",
                             "\u0d86",
                             "\u0d85\u0dd0",
                             "\u0d87",
                             "\u0d85\u0dd1",
                             "\u0d88",
                             "\u0d8b\u0ddf",
                             "\u0d8c",
                             "\u0d8d\u0dd8",
                             "\u0d8e",
                             "\u0d91\u0dca",
                             "\u0d92",
                             "\u0dd9\u0d91",
                             "\u0d93",
                             "\u0d94\u0dca",
                             "\u0d95",
                             "\u0d94\u0ddf",
                             "\u0d96",
                             "([\u0d9a-\u0dc6])\u0dd8\u0dd8",
                             "\\1\u0df2",
                             "\u0dd9([\u0d9a-\u0dc6])",
                             "\\1\u0dd9",
                             "([\u0d9a-\u0dc6])\u0dd9\u001d\u0dca",
                             "\\1\u0dda",
                             "\u0dd9\u0dd9([\u0d9a-\u0dc6])",
                             "\\1\u0ddb",
                             "([\u0d9a-\u0dc6])\u0dd9\u001d\u0dcf",
                             "\\1\u0ddc",
                             "([\u0d9a-\u0dc6])\u0ddc\u001d\u0dca",
                             "\\1\u0ddd",
                             "([\u0d9a-\u0dc6])\u0dd9\u001d\u0ddf",
                             "\\1\u0dde",
                             "([\u0d9a-\u0dc6])(\u0dd9)\u001d((\u0dca\u200d["
                             "\u0dba\u0dbb])|(\u0dbb\u0dca\u200d))",
                             "\\1\\3\\2",
                             "([\u0d9a-\u0dc6](\u0dca\u200d[\u0dba\u0dbb])|("
                             "\u0dbb\u0dca\u200d))\u0dd9\u001d\u0dca",
                             "\\1\u0dda",
                             "([\u0d9a-\u0dc6](\u0dca\u200d[\u0dba\u0dbb])|("
                             "\u0dbb\u0dca\u200d))\u0dd9\u001d\u0dcf",
                             "\\1\u0ddc",
                             "([\u0d9a-\u0dc6](\u0dca\u200d[\u0dba\u0dbb])|("
                             "\u0dbb\u0dca\u200d))\u0ddc\u001d\u0dca",
                             "\\1\u0ddd"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace si
