// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ta_phone.h"

#include <iterator>

namespace ta_phone {

const char* kId = "ta_phone";
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
const char* kShift[] = {
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
const char* kAltGr[] = {
    "\u0b82",  // BackQuote
    "\u0bf3",  // Digit1
    "\u0bf4",  // Digit2
    "\u0bf5",  // Digit3
    "\u0bf6",  // Digit4
    "\u0bf7",  // Digit5
    "\u0bf8",  // Digit6
    "\u0bfa",  // Digit7
    "\u0bf0",  // Digit8
    "\u0bf1",  // Digit9
    "\u0bf2",  // Digit0
    "\u0bf9",  // Minus
    "\u0be6",  // Equal
    "\u0be7",  // KeyQ
    "\u0be8",  // KeyW
    "\u0be9",  // KeyE
    "\u0bea",  // KeyR
    "\u0beb",  // KeyT
    "\u0bec",  // KeyY
    "\u0bed",  // KeyU
    "\u0bee",  // KeyI
    "\u0bef",  // KeyO
    "\u0bd0",  // KeyP
    "\u0b83",  // BracketLeft
    "\u0b85",  // BracketRight
    "\u0b86",  // Backslash
    "\u0b87",  // KeyA
    "\u0b88",  // KeyS
    "\u0b89",  // KeyD
    "\u0b8a",  // KeyF
    "\u0b8e",  // KeyG
    "\u0b8f",  // KeyH
    "\u0b90",  // KeyJ
    "\u0b92",  // KeyK
    "\u0b93",  // KeyL
    "\u0b94",  // Semicolon
    "\u0b95",  // Quote
    "\u0b99",  // KeyZ
    "\u0b9a",  // KeyX
    "\u0b9c",  // KeyC
    "\u0b9e",  // KeyV
    "\u0b9f",  // KeyB
    "\u0ba3",  // KeyN
    "\u0ba4",  // KeyM
    "\u0ba8",  // Comma
    "\u0ba9",  // Period
    "\u0baa",  // Slash
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
    "\u0bae",  // BackQuote
    "\u0baf",  // Digit1
    "\u0bb0",  // Digit2
    "\u0bb1",  // Digit3
    "\u0bb2",  // Digit4
    "\u0bb3",  // Digit5
    "\u0bb4",  // Digit6
    "\u0bb5",  // Digit7
    "\u0bb6",  // Digit8
    "\u0bb7",  // Digit9
    "\u0bb8",  // Digit0
    "\u0bb9",  // Minus
    "\u0bbe",  // Equal
    "\u0bbf",  // KeyQ
    "\u0bc0",  // KeyW
    "\u0bc1",  // KeyE
    "\u0bc2",  // KeyR
    "\u0bc6",  // KeyT
    "\u0bc7",  // KeyY
    "\u0bc8",  // KeyU
    "\u0bca",  // KeyI
    "\u0bcb",  // KeyO
    "\u0bcc",  // KeyP
    "\u0bcd",  // BracketLeft
    "\u0bd7",  // BracketRight
    "",        // Backslash
    "",        // KeyA
    "",        // KeyS
    "",        // KeyD
    "",        // KeyF
    "",        // KeyG
    "",        // KeyH
    "",        // KeyJ
    "",        // KeyK
    "",        // KeyL
    "",        // Semicolon
    "",        // Quote
    "",        // KeyZ
    "",        // KeyX
    "",        // KeyC
    "",        // KeyV
    "",        // KeyB
    "",        // KeyN
    "",        // KeyM
    "",        // Comma
    "",        // Period
    "",        // Slash
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
const char** kKeyMap[8] = {kNormal,   kShift,         kAltGr, kShiftAltGr,
                           kCapslock, kShiftCapslock, kAltGr, kShiftAltGr};
const char* kTransforms[] = {"\u0bcd\u0bb1\u0bcd\u0bb1\u0bcd\u001d?i",
                             "\u0bcd\u0bb0\u0bbf",
                             "\u0bcd\u0bb1\u0bcd\u001d?\\^i",
                             "\u0bcd\u0bb0\u0bbf",
                             "\u0bcd\u0bb1\u0bcd\u0bb1\u0bcd\u001d?I",
                             "\u0bcd\u0bb0\u0bbf",
                             "\u0bcd\u0bb1\u0bcd\u001d?\\^I",
                             "\u0bcd\u0bb0\u0bbf",
                             "\u0bcd\u0b85\u001d?a",
                             "\u0bbe",
                             "\u0bbf\u001d?i",
                             "\u0bc0",
                             "\u0bc6\u001d?e",
                             "\u0bc0",
                             "\u0bc1\u001d?u",
                             "\u0bc2",
                             "\u0bca\u001d?o",
                             "\u0bc2",
                             "\u0bcd\u0b85\u001d?i",
                             "\u0bc8",
                             "\u0bcd\u0b85\u001d?u",
                             "\u0bcc",
                             "\u0bca\u001d?u",
                             "\u0bcc",
                             "\u0bcd\u001d?a",
                             "",
                             "\u0bcd\u001d?A",
                             "\u0bbe",
                             "\u0bcd\u001d?i",
                             "\u0bbf",
                             "\u0bcd\u001d?I",
                             "\u0bc0",
                             "\u0bcd\u001d?u",
                             "\u0bc1",
                             "\u0bcd\u001d?U",
                             "\u0bc2",
                             "\u0bcd\u001d?e",
                             "\u0bc6",
                             "\u0bcd\u001d?E",
                             "\u0bc7",
                             "\u0bcd\u001d?o",
                             "\u0bca",
                             "\u0bcd\u001d?O",
                             "\u0bcb",
                             "\u0ba9\u0bcd\u001d?ch",
                             "\u0b9e\u0bcd\u0b9a\u0bcd",
                             "\u0b95\u0bcd\u0b9a\u0bcd\u001d?h",
                             "\u0b95\u0bcd\u0bb7\u0bcd",
                             "\u0b9a\u0bcd\u0bb0\u0bcd\u001d?i",
                             "\u0bb8\u0bcd\u0bb0\u0bc0",
                             "\u0b9f\u0bcd\u0b9f\u0bcd\u001d?r",
                             "\u0bb1\u0bcd\u0bb1\u0bcd",
                             "\u0b85\u001d?a",
                             "\u0b86",
                             "\u0b87\u001d?i",
                             "\u0b88",
                             "\u0b8e\u001d?e",
                             "\u0b88",
                             "\u0b89\u001d?u",
                             "\u0b8a",
                             "\u0b92\u001d?o",
                             "\u0b8a",
                             "\u0b85\u001d?i",
                             "\u0b90",
                             "\u0b85\u001d?u",
                             "\u0b94",
                             "\u0b92\u001d?u",
                             "\u0b94",
                             "\u0ba9\u0bcd\u001d?g",
                             "\u0b99\u0bcd",
                             "ch",
                             "\u0b9a\u0bcd",
                             "\u0ba9\u0bcd\u001d?j",
                             "\u0b9e\u0bcd",
                             "\u0b9f\u0bcd\u001d?h",
                             "\u0ba4\u0bcd",
                             "\u0b9a\u0bcd\u001d?h",
                             "\u0bb7\u0bcd",
                             "\u0bb8\u0bcd\u001d?h",
                             "\u0bb6\u0bcd",
                             "\u0bb4\u0bcd\u001d?h",
                             "\u0bb4\u0bcd",
                             "\u0b95\u0bcd\u001d?S",
                             "\u0b95\u0bcd\u0bb7\u0bcd",
                             "\u0b9f\u0bcd\u001d?r",
                             "\u0bb1\u0bcd\u0bb1\u0bcd",
                             "_",
                             "\u200b",
                             "M",
                             "\u0b82",
                             "H",
                             "\u0b83",
                             "a",
                             "\u0b85",
                             "A",
                             "\u0b86",
                             "i",
                             "\u0b87",
                             "I",
                             "\u0b88",
                             "u",
                             "\u0b89",
                             "U",
                             "\u0b8a",
                             "e",
                             "\u0b8e",
                             "E",
                             "\u0b8f",
                             "o",
                             "\u0b92",
                             "O",
                             "\u0b93",
                             "k",
                             "\u0b95\u0bcd",
                             "g",
                             "\u0b95\u0bcd",
                             "q",
                             "\u0b95\u0bcd",
                             "G",
                             "\u0b95\u0bcd",
                             "s",
                             "\u0b9a\u0bcd",
                             "j",
                             "\u0b9c\u0bcd",
                             "J",
                             "\u0b9c\u0bcd",
                             "t",
                             "\u0b9f\u0bcd",
                             "T",
                             "\u0b9f\u0bcd",
                             "d",
                             "\u0b9f\u0bcd",
                             "D",
                             "\u0b9f\u0bcd",
                             "N",
                             "\u0ba3\u0bcd",
                             "n",
                             "\u0ba9\u0bcd",
                             "p",
                             "\u0baa\u0bcd",
                             "b",
                             "\u0baa\u0bcd",
                             "f",
                             "\u0baa\u0bcd",
                             "m",
                             "\u0bae\u0bcd",
                             "y",
                             "\u0baf\u0bcd",
                             "Y",
                             "\u0baf\u0bcd",
                             "r",
                             "\u0bb0\u0bcd",
                             "l",
                             "\u0bb2\u0bcd",
                             "L",
                             "\u0bb3\u0bcd",
                             "v",
                             "\u0bb5\u0bcd",
                             "w",
                             "\u0bb5\u0bcd",
                             "S",
                             "\u0bb8\u0bcd",
                             "h",
                             "\u0bb9\u0bcd",
                             "z",
                             "\u0bb4\u0bcd",
                             "R",
                             "\u0bb1\u0bcd",
                             "x",
                             "\u0b95\u0bcd\u0bb7\u0bcd",
                             "([\u0b95-\u0bb9])\u001d?a",
                             "\\1\u0bbe",
                             "([\u0b95-\u0bb9])\u001d?i",
                             "\\1\u0bc8",
                             "([\u0b95-\u0bb9])\u001d?u",
                             "\\1\u0bcc",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?a",
                             "\\1\u0ba8",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?A",
                             "\\1\u0ba8\u0bbe",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?i",
                             "\\1\u0ba8\u0bbf",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?I",
                             "\\1\u0ba8\u0bc0",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?u",
                             "\\1\u0ba8\u0bc1",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?U",
                             "\\1\u0ba8\u0bc2",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?e",
                             "\\1\u0ba8\u0bc6",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?E",
                             "\\1\u0ba8\u0bc7",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?o",
                             "\\1\u0ba8\u0bca",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?O",
                             "\\1\u0ba8\u0bcb",
                             "\u0ba9\u0bcd\u001d?dha",
                             "\u0ba8\u0bcd\u0ba4",
                             "\u0ba9\u0bcd\u001d?dhA",
                             "\u0ba8\u0bcd\u0ba4\u0bbe",
                             "\u0ba9\u0bcd\u001d?dhi",
                             "\u0ba8\u0bcd\u0ba4\u0bbf",
                             "\u0ba9\u0bcd\u001d?dhI",
                             "\u0ba8\u0bcd\u0ba4\u0bc0",
                             "\u0ba9\u0bcd\u001d?dhu",
                             "\u0ba8\u0bcd\u0ba4\u0bc1",
                             "\u0ba9\u0bcd\u001d?dhU",
                             "\u0ba8\u0bcd\u0ba4\u0bc2",
                             "\u0ba9\u0bcd\u001d?dhe",
                             "\u0ba8\u0bcd\u0ba4\u0bc6",
                             "\u0ba9\u0bcd\u001d?dhE",
                             "\u0ba8\u0bcd\u0ba4\u0bc7",
                             "\u0ba9\u0bcd\u001d?dho",
                             "\u0ba8\u0bcd\u0ba4\u0bca",
                             "\u0ba9\u0bcd\u001d?dhO",
                             "\u0ba8\u0bcd\u0ba4\u0bcb",
                             "([\u0b80-\u0bff])\u0ba9\u0bcd\u001d?g",
                             "\\1\u0b99\u0bcd\u0b95\u0bcd",
                             "([\u0b80-\u0bff])\u0ba9\u0bcd\u001d?j",
                             "\\1\u0b9e\u0bcd\u0b9a\u0bcd",
                             "([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?y",
                             "\\1\u0b9e\u0bcd",
                             "\u0ba9\u0bcd\u001d?[dt]",
                             "\u0ba3\u0bcd\u0b9f\u0bcd",
                             "\u0ba3\u0bcd\u0b9f\u0bcd\u001d?h",
                             "\u0ba8\u0bcd\u0ba4\u0bcd",
                             "\u0ba9\u0bcd\u001d?dh",
                             "\u0ba8\u0bcd",
                             "\u0ba9\u0bcd\u001d?tr",
                             "\u0ba9\u0bcd\u0b9f\u0bcd\u0bb0\u0bcd",
                             "\u0ba3\u0bcd\u0b9f\u0bcd\u001d?r",
                             "\u0ba9\u0bcd\u0bb1\u0bcd"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = "t|dh|d";

}  // namespace ta_phone
