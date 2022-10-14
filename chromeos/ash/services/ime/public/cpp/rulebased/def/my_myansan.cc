// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/my_myansan.h"

#include <iterator>

namespace my_myansan {

const char* kId = "my_myansan";
bool kIs102 = false;
const char* kNormal[] = {
    "`",             // BackQuote
    "\u1041",        // Digit1
    "\u1042",        // Digit2
    "\u1043",        // Digit3
    "\u1044",        // Digit4
    "\u1045",        // Digit5
    "\u1046",        // Digit6
    "\u1047",        // Digit7
    "\u1048",        // Digit8
    "\u1049",        // Digit9
    "\u1040",        // Digit0
    "-",             // Minus
    "=",             // Equal
    "\u1006",        // KeyQ
    "\u1010",        // KeyW
    "\u1014",        // KeyE
    "\u1019",        // KeyR
    "\u1021",        // KeyT
    "\u1015",        // KeyY
    "\u1000",        // KeyU
    "\u1004",        // KeyI
    "\u101e",        // KeyO
    "\u1005",        // KeyP
    "\u101f",        // BracketLeft
    "\u1029",        // BracketRight
    "\u104f",        // Backslash
    "\u200c\u1031",  // KeyA
    "\u103b",        // KeyS
    "\u102d",        // KeyD
    "\u103a",        // KeyF
    "\u102b",        // KeyG
    "\u1037",        // KeyH
    "\u103c",        // KeyJ
    "\u102f",        // KeyK
    "\u1030",        // KeyL
    "\u1038",        // Semicolon
    "'",             // Quote
    "\u1016",        // KeyZ
    "\u1011",        // KeyX
    "\u1001",        // KeyC
    "\u101c",        // KeyV
    "\u1018",        // KeyB
    "\u100a",        // KeyN
    "\u102c",        // KeyM
    "\u101a",        // Comma
    ".",             // Period
    "\u104b",        // Slash
    "\u0020",        // Space
};
const char* kShift[] = {
    "\u100e",                    // BackQuote
    "\u100d",                    // Digit1
    "\u100f\u1039\u100d",        // Digit2
    "\u100b",                    // Digit3
    "\u1000\u103b\u1015\u103a",  // Digit4
    "%",                         // Digit5
    "/",                         // Digit6
    "\u101b",                    // Digit7
    "\u1002",                    // Digit8
    "(",                         // Digit9
    ")",                         // Digit0
    "x",                         // Minus
    "+",                         // Equal
    "\u1008",                    // KeyQ
    "\u101d",                    // KeyW
    "\u1023",                    // KeyE
    "\u104e",                    // KeyR
    "\u1024",                    // KeyT
    "\u104c",                    // KeyY
    "\u1025",                    // KeyU
    "\u104d",                    // KeyI
    "\u103f",                    // KeyO
    "\u100f",                    // KeyP
    "\u1027",                    // BracketLeft
    "\u102a",                    // BracketRight
    "\u100b\u1039\u100c",        // Backslash
    "\u1017",                    // KeyA
    "\u103e",                    // KeyS
    "\u102e",                    // KeyD
    "\u1039",                    // KeyF
    "\u103d",                    // KeyG
    "\u1036",                    // KeyH
    "\u1032",                    // KeyJ
    "\u1012",                    // KeyK
    "\u1013",                    // KeyL
    "\u1038",                    // Semicolon
    "\"",                        // Quote
    "\u1007",                    // KeyZ
    "\u100c",                    // KeyX
    "\u1003",                    // KeyC
    "\u1020",                    // KeyV
    "\u101a",                    // KeyB
    "\u1009",                    // KeyN
    "\u1026",                    // KeyM
    "\u101a",                    // Comma
    ".",                         // Period
    "\u104a",                    // Slash
    "\u0020",                    // Space
};
const char* kAltGr[] = {
    "`",             // BackQuote
    "\u1041",        // Digit1
    "\u1042",        // Digit2
    "\u1043",        // Digit3
    "\u1044",        // Digit4
    "\u1045",        // Digit5
    "\u1046",        // Digit6
    "\u1047",        // Digit7
    "\u1048",        // Digit8
    "\u1049",        // Digit9
    "\u1040",        // Digit0
    "-",             // Minus
    "=",             // Equal
    "\u1006",        // KeyQ
    "\u1010",        // KeyW
    "\u1014",        // KeyE
    "\u1019",        // KeyR
    "\u1021",        // KeyT
    "\u1015",        // KeyY
    "\u1000",        // KeyU
    "\u1004",        // KeyI
    "\u101e",        // KeyO
    "\u1005",        // KeyP
    "\u101f",        // BracketLeft
    "\u1029",        // BracketRight
    "\u104f",        // Backslash
    "\u200c\u1031",  // KeyA
    "\u103b",        // KeyS
    "\u102d",        // KeyD
    "\u103a",        // KeyF
    "\u102b",        // KeyG
    "\u1037",        // KeyH
    "\u103c",        // KeyJ
    "\u102f",        // KeyK
    "\u1030",        // KeyL
    "\u1038",        // Semicolon
    "'",             // Quote
    "\u1016",        // KeyZ
    "\u1011",        // KeyX
    "\u1001",        // KeyC
    "\u101c",        // KeyV
    "\u1018",        // KeyB
    "\u100a",        // KeyN
    "\u102c",        // KeyM
    "\u101a",        // Comma
    ".",             // Period
    "\u104b",        // Slash
    "\u0020",        // Space
};
const char* kCapslock[] = {
    "`",       // BackQuote
    "1",       // Digit1
    "2",       // Digit2
    "3",       // Digit3
    "4",       // Digit4
    "5",       // Digit5
    "6",       // Digit6
    "7",       // Digit7
    "8",       // Digit8
    "9",       // Digit9
    "0",       // Digit0
    "-",       // Minus
    "=",       // Equal
    "q",       // KeyQ
    "w",       // KeyW
    "e",       // KeyE
    "r",       // KeyR
    "t",       // KeyT
    "y",       // KeyY
    "u",       // KeyU
    "i",       // KeyI
    "o",       // KeyO
    "p",       // KeyP
    "[",       // BracketLeft
    "]",       // BracketRight
    "\\",      // Backslash
    "a",       // KeyA
    "s",       // KeyS
    "d",       // KeyD
    "f",       // KeyF
    "g",       // KeyG
    "h",       // KeyH
    "j",       // KeyJ
    "k",       // KeyK
    "l",       // KeyL
    ";",       // Semicolon
    "'",       // Quote
    "z",       // KeyZ
    "x",       // KeyX
    "c",       // KeyC
    "v",       // KeyV
    "b",       // KeyB
    "n",       // KeyN
    "m",       // KeyM
    ",",       // Comma
    ".",       // Period
    "/",       // Slash
    "\u0020",  // Space
};
const char* kShiftAltGr[] = {
    "\u100e",                    // BackQuote
    "\u100d",                    // Digit1
    "\u100f\u1039\u100d",        // Digit2
    "\u100b",                    // Digit3
    "\u1000\u103b\u1015\u103a",  // Digit4
    "%",                         // Digit5
    "/",                         // Digit6
    "\u101b",                    // Digit7
    "\u1002",                    // Digit8
    "(",                         // Digit9
    ")",                         // Digit0
    "x",                         // Minus
    "+",                         // Equal
    "\u1008",                    // KeyQ
    "\u101d",                    // KeyW
    "\u1023",                    // KeyE
    "\u104e",                    // KeyR
    "\u1024",                    // KeyT
    "\u104c",                    // KeyY
    "\u1025",                    // KeyU
    "\u104d",                    // KeyI
    "\u103f",                    // KeyO
    "\u100f",                    // KeyP
    "\u1027",                    // BracketLeft
    "\u102a",                    // BracketRight
    "\u100b\u1039\u100c",        // Backslash
    "\u1017",                    // KeyA
    "\u103e",                    // KeyS
    "\u102e",                    // KeyD
    "\u1039",                    // KeyF
    "\u103d",                    // KeyG
    "\u1036",                    // KeyH
    "\u1032",                    // KeyJ
    "\u1012",                    // KeyK
    "\u1013",                    // KeyL
    "\u1038",                    // Semicolon
    "\"",                        // Quote
    "\u1007",                    // KeyZ
    "\u100c",                    // KeyX
    "\u1003",                    // KeyC
    "\u1020",                    // KeyV
    "\u101a",                    // KeyB
    "\u1009",                    // KeyN
    "\u1026",                    // KeyM
    "\u101a",                    // Comma
    ".",                         // Period
    "\u104a",                    // Slash
    "\u0020",                    // Space
};
const char* kAltgrCapslock[] = {
    "`",       // BackQuote
    "1",       // Digit1
    "2",       // Digit2
    "3",       // Digit3
    "4",       // Digit4
    "5",       // Digit5
    "6",       // Digit6
    "7",       // Digit7
    "8",       // Digit8
    "9",       // Digit9
    "0",       // Digit0
    "-",       // Minus
    "=",       // Equal
    "q",       // KeyQ
    "w",       // KeyW
    "e",       // KeyE
    "r",       // KeyR
    "t",       // KeyT
    "y",       // KeyY
    "u",       // KeyU
    "i",       // KeyI
    "o",       // KeyO
    "p",       // KeyP
    "[",       // BracketLeft
    "]",       // BracketRight
    "\\",      // Backslash
    "a",       // KeyA
    "s",       // KeyS
    "d",       // KeyD
    "f",       // KeyF
    "g",       // KeyG
    "h",       // KeyH
    "j",       // KeyJ
    "k",       // KeyK
    "l",       // KeyL
    ";",       // Semicolon
    "'",       // Quote
    "z",       // KeyZ
    "x",       // KeyX
    "c",       // KeyC
    "v",       // KeyV
    "b",       // KeyB
    "n",       // KeyN
    "m",       // KeyM
    ",",       // Comma
    ".",       // Period
    "/",       // Slash
    "\u0020",  // Space
};
const char* kShiftCapslock[] = {
    "~",       // BackQuote
    "!",       // Digit1
    "@",       // Digit2
    "#",       // Digit3
    "$",       // Digit4
    "%",       // Digit5
    "^",       // Digit6
    "&",       // Digit7
    "*",       // Digit8
    "(",       // Digit9
    ")",       // Digit0
    "_",       // Minus
    "+",       // Equal
    "Q",       // KeyQ
    "W",       // KeyW
    "E",       // KeyE
    "R",       // KeyR
    "T",       // KeyT
    "Y",       // KeyY
    "U",       // KeyU
    "I",       // KeyI
    "O",       // KeyO
    "P",       // KeyP
    "{",       // BracketLeft
    "}",       // BracketRight
    "|",       // Backslash
    "A",       // KeyA
    "S",       // KeyS
    "D",       // KeyD
    "F",       // KeyF
    "G",       // KeyG
    "H",       // KeyH
    "J",       // KeyJ
    "K",       // KeyK
    "L",       // KeyL
    ":",       // Semicolon
    "\"",      // Quote
    "Z",       // KeyZ
    "X",       // KeyX
    "C",       // KeyC
    "V",       // KeyV
    "B",       // KeyB
    "N",       // KeyN
    "M",       // KeyM
    "<",       // Comma
    ">",       // Period
    "?",       // Slash
    "\u0020",  // Space
};
const char* kShiftAltGrCapslock[] = {
    "~",       // BackQuote
    "!",       // Digit1
    "@",       // Digit2
    "#",       // Digit3
    "$",       // Digit4
    "%",       // Digit5
    "^",       // Digit6
    "&",       // Digit7
    "*",       // Digit8
    "(",       // Digit9
    ")",       // Digit0
    "_",       // Minus
    "+",       // Equal
    "Q",       // KeyQ
    "W",       // KeyW
    "E",       // KeyE
    "R",       // KeyR
    "T",       // KeyT
    "Y",       // KeyY
    "U",       // KeyU
    "I",       // KeyI
    "O",       // KeyO
    "P",       // KeyP
    "{",       // BracketLeft
    "}",       // BracketRight
    "|",       // Backslash
    "A",       // KeyA
    "S",       // KeyS
    "D",       // KeyD
    "F",       // KeyF
    "G",       // KeyG
    "H",       // KeyH
    "J",       // KeyJ
    "K",       // KeyK
    "L",       // KeyL
    ":",       // Semicolon
    "\"",      // Quote
    "Z",       // KeyZ
    "X",       // KeyX
    "C",       // KeyC
    "V",       // KeyV
    "B",       // KeyB
    "N",       // KeyN
    "M",       // KeyM
    "<",       // Comma
    ">",       // Period
    "?",       // Slash
    "\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};
const char* kTransforms[] = {
    "\u200c\u1031([\u1000-\u102a\u103f\u104e])",
    "\\1\u1031",
    "([\u103c-\u103e]*\u1031)\u001d\u103b",
    "\u103b\\1",
    "([\u103b]*)([\u103d-\u103e]*)\u1031\u001d\u103c",
    "\\1\u103c\\2\u1031",
    "([\u103b\u103c]*)([\u103e]*)\u1031\u001d\u103d",
    "\\1\u103d\\2\u1031",
    "([\u103b-\u103d]*)\u1031\u001d\u103e",
    "\\1\u103e\u1031",
    "([\u103c-\u103e]+)\u001d?\u103b",
    "\u103b\\1",
    "([\u103b]*)([\u103d-\u103e]+)\u001d?\u103c",
    "\\1\u103c\\2",
    "([\u103b\u103c]*)([\u103e]+)\u001d?\u103d",
    "\\1\u103d\\2",
    "\u1004\u1031\u001d\u103a",
    "\u1004\u103a\u1031",
    "\u1004\u103a\u1031\u001d\u1039",
    "\u1004\u103a\u1039\u1031",
    "\u1004\u103a\u1039\u1031\u001d([\u1000-\u102a\u103f\u104e])",
    "\u1004\u103a\u1039\\1\u1031",
    "([\u1000-\u102a\u103f\u104e])\u1031\u001d\u1039",
    "\\1\u1039\u1031",
    "\u1039\u1031\u001d([\u1000-\u1019\u101c\u101e\u1020\u1021])",
    "\u1039\\1\u1031"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace my_myansan
