// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ta_tamil99.h"

#include <iterator>

namespace ta_tamil99 {

const char* kId = "ta_tamil99";
bool kIs102 = false;
const char* kNormal[] = {
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
    "\u0b86",  // KeyQ
    "\u0b88",  // KeyW
    "\u0b8a",  // KeyE
    "\u0b90",  // KeyR
    "\u0b8f",  // KeyT
    "\u0bb3",  // KeyY
    "\u0bb1",  // KeyU
    "\u0ba9",  // KeyI
    "\u0b9f",  // KeyO
    "\u0ba3",  // KeyP
    "\u0b9a",  // BracketLeft
    "\u0b9e",  // BracketRight
    "\\",      // Backslash
    "\u0b85",  // KeyA
    "\u0b87",  // KeyS
    "\u0b89",  // KeyD
    "\u0bcd",  // KeyF
    "\u0b8e",  // KeyG
    "\u0b95",  // KeyH
    "\u0baa",  // KeyJ
    "\u0bae",  // KeyK
    "\u0ba4",  // KeyL
    "\u0ba8",  // Semicolon
    "\u0baf",  // Quote
    "\u0b94",  // KeyZ
    "\u0b93",  // KeyX
    "\u0b92",  // KeyC
    "\u0bb5",  // KeyV
    "\u0b99",  // KeyB
    "\u0bb2",  // KeyN
    "\u0bb0",  // KeyM
    ",",       // Comma
    ".",       // Period
    "\u0bb4",  // Slash
    "\u0020",  // Space
};
const char* kShift[] = {
    "~",                         // BackQuote
    "!",                         // Digit1
    "@",                         // Digit2
    "#",                         // Digit3
    "$",                         // Digit4
    "%",                         // Digit5
    "^",                         // Digit6
    "&",                         // Digit7
    "*",                         // Digit8
    "(",                         // Digit9
    ")",                         // Digit0
    "_",                         // Minus
    "+",                         // Equal
    "\u0bb8",                    // KeyQ
    "\u0bb7",                    // KeyW
    "\u0b9c",                    // KeyE
    "\u0bb9",                    // KeyR
    "\u0bb8\u0bcd\u0bb0\u0bc0",  // KeyT
    "\u0b95\u0bcd\u0bb7",        // KeyY
    "",                          // KeyU
    "",                          // KeyI
    "[",                         // KeyO
    "]",                         // KeyP
    "{",                         // BracketLeft
    "}",                         // BracketRight
    "|",                         // Backslash
    "\u0bf9",                    // KeyA
    "\u0bfa",                    // KeyS
    "\u0bf8",                    // KeyD
    "\u0b83",                    // KeyF
    "",                          // KeyG
    "",                          // KeyH
    "",                          // KeyJ
    "\"",                        // KeyK
    ":",                         // KeyL
    ";",                         // Semicolon
    "'",                         // Quote
    "\u0bf3",                    // KeyZ
    "\u0bf4",                    // KeyX
    "\u0bf5",                    // KeyC
    "\u0bf6",                    // KeyV
    "\u0bf7",                    // KeyB
    "",                          // KeyN
    "/",                         // KeyM
    "<",                         // Comma
    ">",                         // Period
    "?",                         // Slash
    "\u0020",                    // Space
};
const char* kAltGr[] = {
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
    "\u0b86",  // KeyQ
    "\u0b88",  // KeyW
    "\u0b8a",  // KeyE
    "\u0b90",  // KeyR
    "\u0b8f",  // KeyT
    "\u0bb3",  // KeyY
    "\u0bb1",  // KeyU
    "\u0ba9",  // KeyI
    "\u0b9f",  // KeyO
    "\u0ba3",  // KeyP
    "\u0b9a",  // BracketLeft
    "\u0b9e",  // BracketRight
    "\\",      // Backslash
    "\u0b85",  // KeyA
    "\u0b87",  // KeyS
    "\u0b89",  // KeyD
    "\u0bcd",  // KeyF
    "\u0b8e",  // KeyG
    "\u0b95",  // KeyH
    "\u0baa",  // KeyJ
    "\u0bae",  // KeyK
    "\u0ba4",  // KeyL
    "\u0ba8",  // Semicolon
    "\u0baf",  // Quote
    "\u0b94",  // KeyZ
    "\u0b93",  // KeyX
    "\u0b92",  // KeyC
    "\u0bb5",  // KeyV
    "\u0b99",  // KeyB
    "\u0bb2",  // KeyN
    "\u0bb0",  // KeyM
    ",",       // Comma
    ".",       // Period
    "\u0bb4",  // Slash
    "\u0020",  // Space
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
    "[",       // BracketLeft
    "]",       // BracketRight
    "\\",      // Backslash
    "A",       // KeyA
    "S",       // KeyS
    "D",       // KeyD
    "F",       // KeyF
    "G",       // KeyG
    "H",       // KeyH
    "J",       // KeyJ
    "K",       // KeyK
    "L",       // KeyL
    ";",       // Semicolon
    "'",       // Quote
    "Z",       // KeyZ
    "X",       // KeyX
    "C",       // KeyC
    "V",       // KeyV
    "B",       // KeyB
    "N",       // KeyN
    "M",       // KeyM
    ",",       // Comma
    ".",       // Period
    "/",       // Slash
    "\u0020",  // Space
};
const char* kShiftAltGr[] = {
    "~",                         // BackQuote
    "!",                         // Digit1
    "@",                         // Digit2
    "#",                         // Digit3
    "$",                         // Digit4
    "%",                         // Digit5
    "^",                         // Digit6
    "&",                         // Digit7
    "*",                         // Digit8
    "(",                         // Digit9
    ")",                         // Digit0
    "_",                         // Minus
    "+",                         // Equal
    "\u0bb8",                    // KeyQ
    "\u0bb7",                    // KeyW
    "\u0b9c",                    // KeyE
    "\u0bb9",                    // KeyR
    "\u0bb8\u0bcd\u0bb0\u0bc0",  // KeyT
    "\u0b95\u0bcd\u0bb7",        // KeyY
    "",                          // KeyU
    "",                          // KeyI
    "[",                         // KeyO
    "]",                         // KeyP
    "{",                         // BracketLeft
    "}",                         // BracketRight
    "|",                         // Backslash
    "\u0bf9",                    // KeyA
    "\u0bfa",                    // KeyS
    "\u0bf8",                    // KeyD
    "\u0b83",                    // KeyF
    "",                          // KeyG
    "",                          // KeyH
    "",                          // KeyJ
    "\"",                        // KeyK
    ":",                         // KeyL
    ";",                         // Semicolon
    "'",                         // Quote
    "\u0bf3",                    // KeyZ
    "\u0bf4",                    // KeyX
    "\u0bf5",                    // KeyC
    "\u0bf6",                    // KeyV
    "\u0bf7",                    // KeyB
    "",                          // KeyN
    "/",                         // KeyM
    "<",                         // Comma
    ">",                         // Period
    "?",                         // Slash
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
    "[",       // BracketLeft
    "]",       // BracketRight
    "\\",      // Backslash
    "A",       // KeyA
    "S",       // KeyS
    "D",       // KeyD
    "F",       // KeyF
    "G",       // KeyG
    "H",       // KeyH
    "J",       // KeyJ
    "K",       // KeyK
    "L",       // KeyL
    ";",       // Semicolon
    "'",       // Quote
    "Z",       // KeyZ
    "X",       // KeyX
    "C",       // KeyC
    "V",       // KeyV
    "B",       // KeyB
    "N",       // KeyN
    "M",       // KeyM
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
    "{",       // BracketLeft
    "}",       // BracketRight
    "|",       // Backslash
    "a",       // KeyA
    "s",       // KeyS
    "d",       // KeyD
    "f",       // KeyF
    "g",       // KeyG
    "h",       // KeyH
    "j",       // KeyJ
    "k",       // KeyK
    "l",       // KeyL
    ":",       // Semicolon
    "\"",      // Quote
    "z",       // KeyZ
    "x",       // KeyX
    "c",       // KeyC
    "v",       // KeyV
    "b",       // KeyB
    "n",       // KeyN
    "m",       // KeyM
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
    "{",       // BracketLeft
    "}",       // BracketRight
    "|",       // Backslash
    "a",       // KeyA
    "s",       // KeyS
    "d",       // KeyD
    "f",       // KeyF
    "g",       // KeyG
    "h",       // KeyH
    "j",       // KeyJ
    "k",       // KeyK
    "l",       // KeyL
    ":",       // Semicolon
    "\"",      // Quote
    "z",       // KeyZ
    "x",       // KeyX
    "c",       // KeyC
    "v",       // KeyV
    "b",       // KeyB
    "n",       // KeyN
    "m",       // KeyM
    "<",       // Comma
    ">",       // Period
    "?",       // Slash
    "\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};
const char* kTransforms[] = {"([\u0b95-\u0bb9])\u0b85", "\\1\u200d",
                             "([\u0b95-\u0bb9])\u0b86", "\\1\u0bbe",
                             "([\u0b95-\u0bb9])\u0b87", "\\1\u0bbf",
                             "([\u0b95-\u0bb9])\u0b88", "\\1\u0bc0",
                             "([\u0b95-\u0bb9])\u0b89", "\\1\u0bc1",
                             "([\u0b95-\u0bb9])\u0b8a", "\\1\u0bc2",
                             "([\u0b95-\u0bb9])\u0b8e", "\\1\u0bc6",
                             "([\u0b95-\u0bb9])\u0b8f", "\\1\u0bc7",
                             "([\u0b95-\u0bb9])\u0b90", "\\1\u0bc8",
                             "([\u0b95-\u0bb9])\u0b92", "\\1\u0bca",
                             "([\u0b95-\u0bb9])\u0b93", "\\1\u0bcb",
                             "([\u0b95-\u0bb9])\u0b94", "\\1\u0bcc"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace ta_tamil99
