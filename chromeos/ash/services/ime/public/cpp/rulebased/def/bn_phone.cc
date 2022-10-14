// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/bn_phone.h"

#include <iterator>

namespace bn_phone {

const char* kId = "bn_phone";
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
    "\u0982",  // BackQuote
    "\u0981",  // Digit1
    "\u09bc",  // Digit2
    "\u0983",  // Digit3
    "\u09fa",  // Digit4
    "\u09f8",  // Digit5
    "\u09f9",  // Digit6
    "\u09f2",  // Digit7
    "\u09f3",  // Digit8
    "\u09e6",  // Digit9
    "\u09f4",  // Digit0
    "\u09e7",  // Minus
    "\u09f5",  // Equal
    "\u09e8",  // KeyQ
    "\u09f6",  // KeyW
    "\u09e9",  // KeyE
    "\u09f7",  // KeyR
    "\u09ea",  // KeyT
    "\u09eb",  // KeyY
    "\u09ec",  // KeyU
    "\u09ed",  // KeyI
    "\u09ee",  // KeyO
    "\u09ef",  // KeyP
    "\u0985",  // BracketLeft
    "\u0986",  // BracketRight
    "\u0987",  // Backslash
    "\u0988",  // KeyA
    "\u0989",  // KeyS
    "\u098a",  // KeyD
    "\u098b",  // KeyF
    "\u09e0",  // KeyG
    "\u098c",  // KeyH
    "\u09e1",  // KeyJ
    "\u098f",  // KeyK
    "\u0990",  // KeyL
    "\u0993",  // Semicolon
    "\u0994",  // Quote
    "\u0995",  // KeyZ
    "\u0996",  // KeyX
    "\u0997",  // KeyC
    "\u0998",  // KeyV
    "\u0999",  // KeyB
    "\u099a",  // KeyN
    "\u099b",  // KeyM
    "\u099c",  // Comma
    "\u099d",  // Period
    "\u099e",  // Slash
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
    "\u099f",  // BackQuote
    "\u09a0",  // Digit1
    "\u09a1",  // Digit2
    "\u09dc",  // Digit3
    "\u09a2",  // Digit4
    "\u09dd",  // Digit5
    "\u09a3",  // Digit6
    "\u09a4",  // Digit7
    "\u09ce",  // Digit8
    "\u09a5",  // Digit9
    "\u09a6",  // Digit0
    "\u09a7",  // Minus
    "\u09a8",  // Equal
    "\u09aa",  // KeyQ
    "\u09ab",  // KeyW
    "\u09ac",  // KeyE
    "\u09ad",  // KeyR
    "\u09ae",  // KeyT
    "\u09af",  // KeyY
    "\u09df",  // KeyU
    "\u09b0",  // KeyI
    "\u09f0",  // KeyO
    "\u09b2",  // KeyP
    "\u09f1",  // BracketLeft
    "\u09b6",  // BracketRight
    "\u09b7",  // Backslash
    "\u09b8",  // KeyA
    "\u09b9",  // KeyS
    "\u09bd",  // KeyD
    "\u09be",  // KeyF
    "\u09bf",  // KeyG
    "\u09c0",  // KeyH
    "\u09c1",  // KeyJ
    "\u09c2",  // KeyK
    "\u09c3",  // KeyL
    "\u09c4",  // Semicolon
    "\u09e2",  // Quote
    "\u09e3",  // KeyZ
    "\u09c7",  // KeyX
    "\u09c8",  // KeyC
    "\u09cb",  // KeyV
    "\u09cc",  // KeyB
    "\u09cd",  // KeyN
    "\u09d7",  // KeyM
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
const char** kKeyMap[8] = {kNormal,     kShift,        kAltGr,
                           kShiftAltGr, kCapslock,     kShiftCapslock,
                           kCapslock,   kShiftCapslock};
const char* kTransforms[] = {
    "\u09cd\u001d?\\.c",
    "\u09c7",
    "\u0986\u098a\u001d?M",
    "\u0993\u0982",
    "\u09dc\u001d?\\^i",
    "\u098b",
    "\u09dc\u001d?\\^I",
    "\u09e0",
    "\u09b2\u001d?\\^i",
    "\u098c",
    "\u09b2\u001d?\\^I",
    "\u09e1",
    "\u099a\u001d?h",
    "\u099b",
    "\u0995\u09cd\u09b7\u001d?h",
    "\u0995\u09cd\u09b7",
    "\\.n",
    "\u0982",
    "\\.m",
    "\u0982",
    "\\.N",
    "\u0981",
    "\\.h",
    "\u09cd\u200c",
    "\\.a",
    "\u09bd",
    "OM",
    "\u0993\u0982",
    "\u0985\u001d?a",
    "\u0986",
    "\u0987\u001d?i",
    "\u0988",
    "\u098f\u001d?e",
    "\u0988",
    "\u0989\u001d?u",
    "\u098a",
    "\u0993\u001d?o",
    "\u098a",
    "\u0985\u001d?i",
    "\u0990",
    "\u0985\u001d?u",
    "\u0994",
    "\u0995\u001d?h",
    "\u0996",
    "\u0997\u001d?h",
    "\u0998",
    "~N",
    "\u0999",
    "ch",
    "\u099a",
    "Ch",
    "\u099b",
    "\u099c\u001d?h",
    "\u099d",
    "~n",
    "\u099e",
    "\u099f\u001d?h",
    "\u09a0",
    "\u09a1\u001d?h",
    "\u09a2",
    "\u09a4\u001d?h",
    "\u09a5",
    "\u09a6\u001d?h",
    "\u09a7",
    "\u09aa\u001d?h",
    "\u09ab",
    "\u09ac\u001d?h",
    "\u09ad",
    "\u09b8\u001d?h",
    "\u09b6",
    "\u09b6\u001d?h",
    "\u09b7",
    "~h",
    "\u09cd\u09b9",
    "\u09dc\u001d?h",
    "\u09dd",
    "\u0995\u001d?S",
    "\u0995\u09cd\u09b7",
    "GY",
    "\u099c\u09cd\u099e",
    "M",
    "\u0982",
    "H",
    "\u0983",
    "a",
    "\u0985",
    "A",
    "\u0986",
    "i",
    "\u0987",
    "I",
    "\u0988",
    "u",
    "\u0989",
    "U",
    "\u098a",
    "e",
    "\u098f",
    "o",
    "\u0993",
    "k",
    "\u0995",
    "g",
    "\u0997",
    "j",
    "\u099c",
    "T",
    "\u099f",
    "D",
    "\u09a1",
    "N",
    "\u09a3",
    "t",
    "\u09a4",
    "d",
    "\u09a6",
    "n",
    "\u09a8",
    "p",
    "\u09aa",
    "b",
    "\u09ac",
    "m",
    "\u09ae",
    "y",
    "\u09af",
    "r",
    "\u09b0",
    "l",
    "\u09b2",
    "L",
    "\u09b2",
    "v",
    "\u09ac",
    "w",
    "\u09ac",
    "S",
    "\u09b6",
    "s",
    "\u09b8",
    "h",
    "\u09b9",
    "R",
    "\u09dc",
    "Y",
    "\u09df",
    "x",
    "\u0995\u09cd\u09b7",
    "([\u0995-\u09b9\u09dc-\u09df])\u001da",
    "\\1",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daa",
    "\\1\u09be",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dai",
    "\\1\u09c8",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dau",
    "\\1\u09cc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dA",
    "\\1\u09be",
    "([\u0995-\u09b9\u09dc-\u09df])\u001di",
    "\\1\u09bf",
    "\u09bf\u001di",
    "\u09c0",
    "\u09c7\u001de",
    "\u09c0",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dI",
    "\\1\u09c0",
    "([\u0995-\u09b9\u09dc-\u09df])\u001du",
    "\\1\u09c1",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dU",
    "\\1\u09c2",
    "\u09c1\u001du",
    "\u09c2",
    "\u09cb\u001do",
    "\u09c2",
    "([\u0995-\u09b9\u09dc-\u09df])"
    "\u09cd\u09b0\u09bc\u09cd\u09b0\u09bc\u001di",
    "\\1\u09c3",
    "([\u0995-\u09b9\u09dc-\u09df])\u09cd\u09b0\u09bc^i",
    "\\1\u09c3",
    "([\u0995-\u09b9\u09dc-\u09df])"
    "\u09cd\u09b0\u09bc\u09cd\u09b0\u09bc\u001dI",
    "\\1\u09c4",
    "([\u0995-\u09b9\u09dc-\u09df])\u09cd\u09b0\u09bc^I",
    "\\1\u09c4",
    "\u09b0\u09bc\u09cd\u09b0\u09bc\u001di",
    "\u098b",
    "\u09b0\u09bc\u09cd\u09b0\u09bc\u001dI",
    "\u09e0",
    "\u09b2\u09cd\u09b2\u001di",
    "\u098c",
    "\u09b2\u09cd\u09b2\u001dI",
    "\u09e1",
    "([\u0995-\u09b9\u09dc-\u09df])\u001de",
    "\\1\u09c7",
    "([\u0995-\u09b9\u09dc-\u09df])\u001do",
    "\\1\u09cb",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dk",
    "\\1\u09cd\u0995",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dg",
    "\\1\u09cd\u0997",
    "([\u0995-\u09b9\u09dc-\u09df])\u001d~N",
    "\\1\u09cd\u0999",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dch",
    "\\1\u09cd\u099a",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dCh",
    "\\1\u09cd\u099b",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dj",
    "\\1\u09cd\u099c",
    "([\u0995-\u09b9\u09dc-\u09df])\u001d~n",
    "\\1\u09cd\u099e",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dT",
    "\\1\u09cd\u099f",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dD",
    "\\1\u09cd\u09a1",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dN",
    "\\1\u09cd\u09a3",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dt",
    "\\1\u09cd\u09a4",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dd",
    "\\1\u09cd\u09a6",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dn",
    "\\1\u09cd\u09a8",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dp",
    "\\1\u09cd\u09aa",
    "([\u0995-\u09b9\u09dc-\u09df])\u001db",
    "\\1\u09cd\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dm",
    "\\1\u09cd\u09ae",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dr",
    "\\1\u09cd\u09b0",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dl",
    "\\1\u09cd\u09b2",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dL",
    "\\1\u09cd\u09b2",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dv",
    "\\1\u09cd\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dw",
    "\\1\u09cd\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dS",
    "\\1\u09cd\u09b6",
    "([\u0995-\u09b9\u09dc-\u09df])\u001ds",
    "\\1\u09cd\u09b8",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dh",
    "\\1\u09cd\u09b9",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dR",
    "\\1\u09cd\u09b0\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dq",
    "\\1\u09cd\u0995\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dKh",
    "\\1\u09cd\u0996\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dG",
    "\\1\u09cd\u0997\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dz",
    "\\1\u09cd\u099c\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dJ",
    "\\1\u09cd\u099c\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001d.D",
    "\\1\u09cd\u09a1\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001df",
    "\\1\u09cd\u09ab\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dy",
    "\\1\u09cd\u09af\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dx",
    "\\1\u09cd\u0995\u09cd\u09b7",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dak",
    "\\1\u0995",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dag",
    "\\1\u0997",
    "([\u0995-\u09b9\u09dc-\u09df])\u001da~N",
    "\\1\u0999",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dach",
    "\\1\u099a",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daCh",
    "\\1\u099b",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daj",
    "\\1\u099c",
    "([\u0995-\u09b9\u09dc-\u09df])\u001da~n",
    "\\1\u099e",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daT",
    "\\1\u099f",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daD",
    "\\1\u09a1",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daN",
    "\\1\u09a3",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dat",
    "\\1\u09a4",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dad",
    "\\1\u09a6",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dan",
    "\\1\u09a8",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dap",
    "\\1\u09aa",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dab",
    "\\1\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dam",
    "\\1\u09ae",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dar",
    "\\1\u09b0",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dal",
    "\\1\u09b2",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daL",
    "\\1\u09b2",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dav",
    "\\1\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daw",
    "\\1\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daS",
    "\\1\u09b6",
    "([\u0995-\u09b9\u09dc-\u09df])\u001das",
    "\\1\u09b8",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dah",
    "\\1\u09b9",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daR",
    "\\1\u09b0\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daq",
    "\\1\u0995\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daKh",
    "\\1\u0996\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daG",
    "\\1\u0997\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daz",
    "\\1\u099c\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daJ",
    "\\1\u099c\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001da.D",
    "\\1\u09a1\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daf",
    "\\1\u09ab\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001day",
    "\\1\u09af\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001dax",
    "\\1\u0995\u09cd\u09b7",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daak",
    "\\1\u09be\u0995",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daag",
    "\\1\u09be\u0997",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daa~N",
    "\\1\u09be\u0999",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daach",
    "\\1\u09be\u099a",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaCh",
    "\\1\u09be\u099b",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaj",
    "\\1\u09be\u099c",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daa~n",
    "\\1\u09be\u099e",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaT",
    "\\1\u09be\u099f",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaD",
    "\\1\u09be\u09a1",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaN",
    "\\1\u09be\u09a3",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daat",
    "\\1\u09be\u09a4",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daad",
    "\\1\u09be\u09a6",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daan",
    "\\1\u09be\u09a8",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daap",
    "\\1\u09be\u09aa",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daab",
    "\\1\u09be\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daam",
    "\\1\u09be\u09ae",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daar",
    "\\1\u09be\u09b0",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daal",
    "\\1\u09be\u09b2",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaL",
    "\\1\u09be\u09b2",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daav",
    "\\1\u09be\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaw",
    "\\1\u09be\u09ac",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaS",
    "\\1\u09be\u09b6",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daas",
    "\\1\u09be\u09b8",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daah",
    "\\1\u09be\u09b9",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaR",
    "\\1\u09be\u09b0\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaq",
    "\\1\u09be\u0995\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaKh",
    "\\1\u09be\u0996\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaG",
    "\\1\u09be\u0997\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaz",
    "\\1\u09be\u099c\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaJ",
    "\\1\u09be\u099c\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daa.D",
    "\\1\u09be\u09a1\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daaf",
    "\\1\u09be\u09ab\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daay",
    "\\1\u09be\u09af\u09bc",
    "([\u0995-\u09b9\u09dc-\u09df])\u001daax",
    "\\1\u09be\u0995\u09cd\u09b7"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune =
    "a|aa|ac|aaC|aac|a\\.|aK|aC|aaK|aS|aaS|aa~|aa\\.|a~";

}  // namespace bn_phone
