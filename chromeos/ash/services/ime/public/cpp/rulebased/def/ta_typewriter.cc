// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ta_typewriter.h"

namespace ta_typewriter {

const char* kId = "ta_typewriter";
bool kIs102 = false;
const char* kNormal[] = {
    "\u0b83",              // BackQuote
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
    "/",                   // Minus
    "=",                   // Equal
    "\u0ba3\u0bc1",        // KeyQ
    "\u0bb1",              // KeyW
    "\u0ba8",              // KeyE
    "\u0b9a",              // KeyR
    "\u0bb5",              // KeyT
    "\u0bb2",              // KeyY
    "\u0bb0",              // KeyU
    "\u0bc8",              // KeyI
    "\u0b9f\u0bbf",        // KeyO
    "\u0bbf",              // KeyP
    "\u0bc1",              // BracketLeft
    "\u0bb9",              // BracketRight
    "\u0b95\u0bcd\u0bb7",  // Backslash
    "\u0baf",              // KeyA
    "\u0bb3",              // KeyS
    "\u0ba9",              // KeyD
    "\u0b95",              // KeyF
    "\u0baa",              // KeyG
    "\u0bbe",              // KeyH
    "\u0ba4",              // KeyJ
    "\u0bae",              // KeyK
    "\u0b9f",              // KeyL
    "\u0bcd",              // Semicolon
    "\u0b99",              // Quote
    "\u0ba3",              // KeyZ
    "\u0b92",              // KeyX
    "\u0b89",              // KeyC
    "\u0b8e",              // KeyV
    "\u0bc6",              // KeyB
    "\u0bc7",              // KeyN
    "\u0b85",              // KeyM
    "\u0b87",              // Comma
    ",",                   // Period
    ".",                   // Slash
    "\u0020",              // Space
};
const char* kShift[] = {
    "'",                         // BackQuote
    "\u0bb8",                    // Digit1
    "\"",                        // Digit2
    "%",                         // Digit3
    "\u0b9c",                    // Digit4
    "\u0bb6",                    // Digit5
    "\u0bb7",                    // Digit6
    "",                          // Digit7
    "",                          // Digit8
    "(",                         // Digit9
    ")",                         // Digit0
    "\u0bb8\u0bcd\u0bb0\u0bc0",  // Minus
    "+",                         // Equal
    "",                          // KeyQ
    "\u0bb1\u0bc1",              // KeyW
    "\u0ba8\u0bc1",              // KeyE
    "\u0b9a\u0bc1",              // KeyR
    "\u0b95\u0bc2",              // KeyT
    "\u0bb2\u0bc1",              // KeyY
    "\u0bb0\u0bc1",              // KeyU
    "\u0b90",                    // KeyI
    "\u0b9f\u0bc0",              // KeyO
    "\u0bc0",                    // KeyP
    "\u0bc2",                    // BracketLeft
    "\u0bcc",                    // BracketRight
    "\u0bf8",                    // Backslash
    "",                          // KeyA
    "\u0bb3\u0bc1",              // KeyS
    "\u0ba9\u0bc1",              // KeyD
    "\u0b95\u0bc1",              // KeyF
    "\u0bb4\u0bc1",              // KeyG
    "\u0bb4",                    // KeyH
    "\u0ba4\u0bc1",              // KeyJ
    "\u0bae\u0bc1",              // KeyK
    "\u0b9f\u0bc1",              // KeyL
    "\\",                        // Semicolon
    "\u0b9e",                    // Quote
    "\u0bb7",                    // KeyZ
    "\u0b93",                    // KeyX
    "\u0b8a",                    // KeyC
    "\u0b8f",                    // KeyV
    "\u0b95\u0bcd\u0bb7",        // KeyB
    "\u0b9a\u0bc2",              // KeyN
    "\u0b86",                    // KeyM
    "\u0b88",                    // Comma
    "?",                         // Period
    "-",                         // Slash
    "\u0020",                    // Space
};
const char* kCapslock[] = {
    "'",                         // BackQuote
    "\u0bb8",                    // Digit1
    "\"",                        // Digit2
    "%",                         // Digit3
    "\u0b9c",                    // Digit4
    "\u0bb6",                    // Digit5
    "\u0bb7",                    // Digit6
    "",                          // Digit7
    "",                          // Digit8
    "(",                         // Digit9
    ")",                         // Digit0
    "\u0bb8\u0bcd\u0bb0\u0bc0",  // Minus
    "+",                         // Equal
    "",                          // KeyQ
    "\u0bb1\u0bc1",              // KeyW
    "\u0ba8\u0bc1",              // KeyE
    "\u0b9a\u0bc1",              // KeyR
    "\u0b95\u0bc2",              // KeyT
    "\u0bb2\u0bc1",              // KeyY
    "\u0bb0\u0bc1",              // KeyU
    "\u0b90",                    // KeyI
    "\u0b9f\u0bc0",              // KeyO
    "\u0bc0",                    // KeyP
    "\u0bc2",                    // BracketLeft
    "\u0bcc",                    // BracketRight
    "\u0bf8",                    // Backslash
    "",                          // KeyA
    "\u0bb3\u0bc1",              // KeyS
    "\u0ba9\u0bc1",              // KeyD
    "\u0b95\u0bc1",              // KeyF
    "\u0bb4\u0bc1",              // KeyG
    "\u0bb4",                    // KeyH
    "\u0ba4\u0bc1",              // KeyJ
    "\u0bae\u0bc1",              // KeyK
    "\u0b9f\u0bc1",              // KeyL
    "\\",                        // Semicolon
    "\u0b9e",                    // Quote
    "\u0bb7",                    // KeyZ
    "\u0b93",                    // KeyX
    "\u0b8a",                    // KeyC
    "\u0b8f",                    // KeyV
    "\u0b95\u0bcd\u0bb7",        // KeyB
    "\u0b9a\u0bc2",              // KeyN
    "\u0b86",                    // KeyM
    "\u0b88",                    // Comma
    "?",                         // Period
    "-",                         // Slash
    "\u0020",                    // Space
};
const char* kShiftCapslock[] = {
    "\u0b83",              // BackQuote
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
    "/",                   // Minus
    "=",                   // Equal
    "\u0ba3\u0bc1",        // KeyQ
    "\u0bb1",              // KeyW
    "\u0ba8",              // KeyE
    "\u0b9a",              // KeyR
    "\u0bb5",              // KeyT
    "\u0bb2",              // KeyY
    "\u0bb0",              // KeyU
    "\u0bc8",              // KeyI
    "\u0b9f\u0bbf",        // KeyO
    "\u0bbf",              // KeyP
    "\u0bc1",              // BracketLeft
    "\u0bb9",              // BracketRight
    "\u0b95\u0bcd\u0bb7",  // Backslash
    "\u0baf",              // KeyA
    "\u0bb3",              // KeyS
    "\u0ba9",              // KeyD
    "\u0b95",              // KeyF
    "\u0baa",              // KeyG
    "\u0bbe",              // KeyH
    "\u0ba4",              // KeyJ
    "\u0bae",              // KeyK
    "\u0b9f",              // KeyL
    "\u0bcd",              // Semicolon
    "\u0b99",              // Quote
    "\u0ba3",              // KeyZ
    "\u0b92",              // KeyX
    "\u0b89",              // KeyC
    "\u0b8e",              // KeyV
    "\u0bc6",              // KeyB
    "\u0bc7",              // KeyN
    "\u0b85",              // KeyM
    "\u0b87",              // Comma
    ",",                   // Period
    ".",                   // Slash
    "\u0020",              // Space
};
const char** kKeyMap[8] = {kNormal,   kShift,        kNormal,
                           kShift,    kCapslock,     kShiftCapslock,
                           kCapslock, kShiftCapslock};

}  // namespace ta_typewriter
