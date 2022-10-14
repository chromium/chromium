// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/gu_phone.h"

#include <iterator>

namespace gu_phone {

const char* kId = "gu_phone";
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
    "\u0af1",              // BackQuote
    "\u0ae6",              // Digit1
    "\u0ae7",              // Digit2
    "\u0ae8",              // Digit3
    "\u0ae9",              // Digit4
    "\u0aea",              // Digit5
    "\u0aeb",              // Digit6
    "\u0aec",              // Digit7
    "\u0aed",              // Digit8
    "\u0aee",              // Digit9
    "\u0aef",              // Digit0
    "\u0ad0",              // Minus
    "\u0a85",              // Equal
    "\u0a85\u0a82",        // KeyQ
    "\u0a85\u0a83",        // KeyW
    "\u0a86",              // KeyE
    "\u0a87",              // KeyR
    "\u0a88",              // KeyT
    "\u0a89",              // KeyY
    "\u0a8a",              // KeyU
    "\u0a8b",              // KeyI
    "\u0ae0",              // KeyO
    "\u0a8c",              // KeyP
    "\u0ae1",              // BracketLeft
    "\u0a8d",              // BracketRight
    "\u0a8f",              // Backslash
    "\u0a90",              // KeyA
    "\u0a91",              // KeyS
    "\u0a93",              // KeyD
    "\u0a94",              // KeyF
    "\u0a95",              // KeyG
    "\u0a95\u0acd\u0ab7",  // KeyH
    "\u0a96",              // KeyJ
    "\u0a97",              // KeyK
    "\u0a98",              // KeyL
    "\u0a99",              // Semicolon
    "\u0a9a",              // Quote
    "\u0a9b",              // KeyZ
    "\u0a9c",              // KeyX
    "\u0a9c\u0acd\u0a9e",  // KeyC
    "\u0a9d",              // KeyV
    "\u0a9e",              // KeyB
    "\u0a9f",              // KeyN
    "\u0aa0",              // KeyM
    "\u0aa1",              // Comma
    "\u0aa2",              // Period
    "\u0aa3",              // Slash
    "\u0020",              // Space
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
    "\u0aa4",              // BackQuote
    "\u0aa4\u0acd\u0ab0",  // Digit1
    "\u0aa5",              // Digit2
    "\u0aa6",              // Digit3
    "\u0aa7",              // Digit4
    "\u0aa8",              // Digit5
    "\u0aaa",              // Digit6
    "\u0aab",              // Digit7
    "\u0aac",              // Digit8
    "\u0aad",              // Digit9
    "\u0aae",              // Digit0
    "\u0aaf",              // Minus
    "\u0ab0",              // Equal
    "\u0ab2",              // KeyQ
    "\u0ab3",              // KeyW
    "\u0ab5",              // KeyE
    "\u0ab6",              // KeyR
    "\u0ab6\u0acd\u0ab0",  // KeyT
    "\u0ab7",              // KeyY
    "\u0ab8",              // KeyU
    "\u0ab9",              // KeyI
    "\u0abc",              // KeyO
    "\u0a81",              // KeyP
    "\u0a82",              // BracketLeft
    "\u0acd",              // BracketRight
    "\u0abe",              // Backslash
    "\u0abf",              // KeyA
    "\u0ac0",              // KeyS
    "\u0ac1",              // KeyD
    "\u0ac2",              // KeyF
    "\u0ac3",              // KeyG
    "\u0ac4",              // KeyH
    "\u0ae2",              // KeyJ
    "\u0ae3",              // KeyK
    "\u0ac5",              // KeyL
    "\u0ac7",              // Semicolon
    "\u0ac8",              // Quote
    "\u0ac9",              // KeyZ
    "\u0acb",              // KeyX
    "\u0acc",              // KeyC
    "\u0abd",              // KeyV
    "",                    // KeyB
    "",                    // KeyN
    "",                    // KeyM
    "",                    // Comma
    "",                    // Period
    "",                    // Slash
    "\u0020",              // Space
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
    "\u0acd\u001d?\\.c",
    "\u0ac5",
    "\u0a86\u0a8a\u001d?M",
    "\u0ad0",
    "\u0ab0\u0abc\u001d?\\^i",
    "\u0a8b",
    "\u0ab0\u0abc\u001d?\\^I",
    "\u0ae0",
    "\u0ab3\u001d?\\^i",
    "\u0a8c",
    "\u0ab3\u001d?\\^I",
    "\u0ae1",
    "\u0a9a\u001d?h",
    "\u0a9b",
    "\u0aa1\u0abc\u001d?h",
    "\u0aa2\u0abc",
    "\u0a95\u0acd\u0ab7\u001d?h",
    "\u0a95\u0acd\u0ab7",
    "\\.n",
    "\u0a82",
    "\\.m",
    "\u0a82",
    "\\.N",
    "\u0a81",
    "\\.h",
    "\u0acd\u200c",
    "\\.a",
    "\u0abd",
    "OM",
    "\u0ad0",
    "\u0a85\u001d?a",
    "\u0a86",
    "\u0a87\u001d?i",
    "\u0a88",
    "\u0a8f\u001d?e",
    "\u0a88",
    "\u0a89\u001d?u",
    "\u0a8a",
    "\u0a93\u001d?o",
    "\u0a8a",
    "\u0a85\u001d?i",
    "\u0a90",
    "\u0a85\u001d?u",
    "\u0a94",
    "\u0a95\u001d?h",
    "\u0a96",
    "\u0a97\u001d?h",
    "\u0a98",
    "~N",
    "\u0a99",
    "ch",
    "\u0a9a",
    "Ch",
    "\u0a9b",
    "\u0a9c\u001d?h",
    "\u0a9d",
    "~n",
    "\u0a9e",
    "\u0a9f\u001d?h",
    "\u0aa0",
    "\u0aa1\u001d?h",
    "\u0aa2",
    "\u0aa4\u001d?h",
    "\u0aa5",
    "\u0aa6\u001d?h",
    "\u0aa7",
    "\u0aaa\u001d?h",
    "\u0aab",
    "\u0aac\u001d?h",
    "\u0aad",
    "\u0ab8\u001d?h",
    "\u0ab6",
    "\u0ab6\u001d?h",
    "\u0ab7",
    "~h",
    "\u0acd\u0ab9",
    "Kh",
    "\u0a96\u0abc",
    "\\.D",
    "\u0aa1\u0abc",
    "\u0ab0\u0abc\u001d?h",
    "\u0aa2\u0abc",
    "\u0a95\u001d?S",
    "\u0a95\u0acd\u0ab7",
    "\u0a97\u0abc\u001d?Y",
    "\u0a9c\u0acd\u0a9e",
    "M",
    "\u0a82",
    "H",
    "\u0a83",
    "a",
    "\u0a85",
    "A",
    "\u0a86",
    "i",
    "\u0a87",
    "I",
    "\u0a88",
    "u",
    "\u0a89",
    "U",
    "\u0a8a",
    "e",
    "\u0a8f",
    "o",
    "\u0a93",
    "k",
    "\u0a95",
    "g",
    "\u0a97",
    "j",
    "\u0a9c",
    "T",
    "\u0a9f",
    "D",
    "\u0aa1",
    "N",
    "\u0aa3",
    "t",
    "\u0aa4",
    "d",
    "\u0aa6",
    "n",
    "\u0aa8",
    "p",
    "\u0aaa",
    "b",
    "\u0aac",
    "m",
    "\u0aae",
    "y",
    "\u0aaf",
    "r",
    "\u0ab0",
    "l",
    "\u0ab2",
    "L",
    "\u0ab3",
    "v",
    "\u0ab5",
    "w",
    "\u0ab5",
    "S",
    "\u0ab6",
    "s",
    "\u0ab8",
    "h",
    "\u0ab9",
    "R",
    "\u0ab0\u0abc",
    "q",
    "\u0a95\u0abc",
    "G",
    "\u0a97\u0abc",
    "z",
    "\u0a9c\u0abc",
    "J",
    "\u0a9c\u0abc",
    "f",
    "\u0aab\u0abc",
    "Y",
    "\u0aaf\u0abc",
    "x",
    "\u0a95\u0acd\u0ab7",
    "([\u0a95-\u0ab9])\u001da",
    "\\1",
    "([\u0a95-\u0ab9])\u001daa",
    "\\1\u0abe",
    "([\u0a95-\u0ab9])\u001dai",
    "\\1\u0ac8",
    "([\u0a95-\u0ab9])\u001dau",
    "\\1\u0acc",
    "([\u0a95-\u0ab9])\u001dA",
    "\\1\u0abe",
    "([\u0a95-\u0ab9])\u001di",
    "\\1\u0abf",
    "\u0abf\u001di",
    "\u0ac0",
    "\u0ac7\u001de",
    "\u0ac0",
    "([\u0a95-\u0ab9])\u001dI",
    "\\1\u0ac0",
    "([\u0a95-\u0ab9])\u001du",
    "\\1\u0ac1",
    "([\u0a95-\u0ab9])\u001dU",
    "\\1\u0ac2",
    "\u0ac1\u001du",
    "\u0ac2",
    "\u0acb\u001do",
    "\u0ac2",
    "([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001di",
    "\\1\u0ac3",
    "([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc^i",
    "\\1\u0ac3",
    "([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001dI",
    "\\1\u0ac4",
    "([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc^I",
    "\\1\u0ac4",
    "\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001di",
    "\u0a8b",
    "\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001dI",
    "\u0ae0",
    "\u0ab3\u0acd\u0ab3\u001di",
    "\u0a8c",
    "\u0ab3\u0acd\u0ab3\u001dI",
    "\u0ae1",
    "([\u0a95-\u0ab9])\u001de",
    "\\1\u0ac7",
    "([\u0a95-\u0ab9])\u001do",
    "\\1\u0acb",
    "([\u0a95-\u0ab9])\u001dk",
    "\\1\u0acd\u0a95",
    "([\u0a95-\u0ab9])\u001dg",
    "\\1\u0acd\u0a97",
    "([\u0a95-\u0ab9])\u001d~N",
    "\\1\u0acd\u0a99",
    "([\u0a95-\u0ab9])\u001dch",
    "\\1\u0acd\u0a9a",
    "([\u0a95-\u0ab9])\u001dCh",
    "\\1\u0acd\u0a9b",
    "([\u0a95-\u0ab9])\u001dj",
    "\\1\u0acd\u0a9c",
    "([\u0a95-\u0ab9])\u001d~n",
    "\\1\u0acd\u0a9e",
    "([\u0a95-\u0ab9])\u001dT",
    "\\1\u0acd\u0a9f",
    "([\u0a95-\u0ab9])\u001dD",
    "\\1\u0acd\u0aa1",
    "([\u0a95-\u0ab9])\u001dN",
    "\\1\u0acd\u0aa3",
    "([\u0a95-\u0ab9])\u001dt",
    "\\1\u0acd\u0aa4",
    "([\u0a95-\u0ab9])\u001dd",
    "\\1\u0acd\u0aa6",
    "([\u0a95-\u0ab9])\u001dn",
    "\\1\u0acd\u0aa8",
    "([\u0a95-\u0ab9])\u001dp",
    "\\1\u0acd\u0aaa",
    "([\u0a95-\u0ab9])\u001db",
    "\\1\u0acd\u0aac",
    "([\u0a95-\u0ab9])\u001dm",
    "\\1\u0acd\u0aae",
    "([\u0a95-\u0ab9])\u001dr",
    "\\1\u0acd\u0ab0",
    "([\u0a95-\u0ab9])\u001dl",
    "\\1\u0acd\u0ab2",
    "([\u0a95-\u0ab9])\u001dL",
    "\\1\u0acd\u0ab3",
    "([\u0a95-\u0ab9])\u001dv",
    "\\1\u0acd\u0ab5",
    "([\u0a95-\u0ab9])\u001dw",
    "\\1\u0acd\u0ab5",
    "([\u0a95-\u0ab9])\u001dS",
    "\\1\u0acd\u0ab6",
    "([\u0a95-\u0ab9])\u001ds",
    "\\1\u0acd\u0ab8",
    "([\u0a95-\u0ab9])\u001dh",
    "\\1\u0acd\u0ab9",
    "([\u0a95-\u0ab9])\u001dR",
    "\\1\u0acd\u0ab0\u0abc",
    "([\u0a95-\u0ab9])\u001dq",
    "\\1\u0acd\u0a95\u0abc",
    "([\u0a95-\u0ab9])\u001dKh",
    "\\1\u0acd\u0a96\u0abc",
    "([\u0a95-\u0ab9])\u001dG",
    "\\1\u0acd\u0a97\u0abc",
    "([\u0a95-\u0ab9])\u001dz",
    "\\1\u0acd\u0a9c\u0abc",
    "([\u0a95-\u0ab9])\u001dJ",
    "\\1\u0acd\u0a9c\u0abc",
    "([\u0a95-\u0ab9])\u001d.D",
    "\\1\u0acd\u0aa1\u0abc",
    "([\u0a95-\u0ab9])\u001df",
    "\\1\u0acd\u0aab\u0abc",
    "([\u0a95-\u0ab9])\u001dy",
    "\\1\u0acd\u0aaf\u0abc",
    "([\u0a95-\u0ab9])\u001dx",
    "\\1\u0acd\u0a95\u0acd\u0ab7",
    "([\u0a95-\u0ab9])\u001dak",
    "\\1\u0a95",
    "([\u0a95-\u0ab9])\u001dag",
    "\\1\u0a97",
    "([\u0a95-\u0ab9])\u001da~N",
    "\\1\u0a99",
    "([\u0a95-\u0ab9])\u001dach",
    "\\1\u0a9a",
    "([\u0a95-\u0ab9])\u001daCh",
    "\\1\u0a9b",
    "([\u0a95-\u0ab9])\u001daj",
    "\\1\u0a9c",
    "([\u0a95-\u0ab9])\u001da~n",
    "\\1\u0a9e",
    "([\u0a95-\u0ab9])\u001daT",
    "\\1\u0a9f",
    "([\u0a95-\u0ab9])\u001daD",
    "\\1\u0aa1",
    "([\u0a95-\u0ab9])\u001daN",
    "\\1\u0aa3",
    "([\u0a95-\u0ab9])\u001dat",
    "\\1\u0aa4",
    "([\u0a95-\u0ab9])\u001dad",
    "\\1\u0aa6",
    "([\u0a95-\u0ab9])\u001dan",
    "\\1\u0aa8",
    "([\u0a95-\u0ab9])\u001dap",
    "\\1\u0aaa",
    "([\u0a95-\u0ab9])\u001dab",
    "\\1\u0aac",
    "([\u0a95-\u0ab9])\u001dam",
    "\\1\u0aae",
    "([\u0a95-\u0ab9])\u001dar",
    "\\1\u0ab0",
    "([\u0a95-\u0ab9])\u001dal",
    "\\1\u0ab2",
    "([\u0a95-\u0ab9])\u001daL",
    "\\1\u0ab3",
    "([\u0a95-\u0ab9])\u001dav",
    "\\1\u0ab5",
    "([\u0a95-\u0ab9])\u001daw",
    "\\1\u0ab5",
    "([\u0a95-\u0ab9])\u001daS",
    "\\1\u0ab6",
    "([\u0a95-\u0ab9])\u001das",
    "\\1\u0ab8",
    "([\u0a95-\u0ab9])\u001dah",
    "\\1\u0ab9",
    "([\u0a95-\u0ab9])\u001daR",
    "\\1\u0ab0\u0abc",
    "([\u0a95-\u0ab9])\u001daq",
    "\\1\u0a95\u0abc",
    "([\u0a95-\u0ab9])\u001daKh",
    "\\1\u0a96\u0abc",
    "([\u0a95-\u0ab9])\u001daG",
    "\\1\u0a97\u0abc",
    "([\u0a95-\u0ab9])\u001daz",
    "\\1\u0a9c\u0abc",
    "([\u0a95-\u0ab9])\u001daJ",
    "\\1\u0a9c\u0abc",
    "([\u0a95-\u0ab9])\u001da.D",
    "\\1\u0aa1\u0abc",
    "([\u0a95-\u0ab9])\u001daf",
    "\\1\u0aab\u0abc",
    "([\u0a95-\u0ab9])\u001day",
    "\\1\u0aaf\u0abc",
    "([\u0a95-\u0ab9])\u001dax",
    "\\1\u0a95\u0acd\u0ab7",
    "([\u0a95-\u0ab9])\u001daak",
    "\\1\u0abe\u0a95",
    "([\u0a95-\u0ab9])\u001daag",
    "\\1\u0abe\u0a97",
    "([\u0a95-\u0ab9])\u001daa~N",
    "\\1\u0abe\u0a99",
    "([\u0a95-\u0ab9])\u001daach",
    "\\1\u0abe\u0a9a",
    "([\u0a95-\u0ab9])\u001daaCh",
    "\\1\u0abe\u0a9b",
    "([\u0a95-\u0ab9])\u001daaj",
    "\\1\u0abe\u0a9c",
    "([\u0a95-\u0ab9])\u001daa~n",
    "\\1\u0abe\u0a9e",
    "([\u0a95-\u0ab9])\u001daaT",
    "\\1\u0abe\u0a9f",
    "([\u0a95-\u0ab9])\u001daaD",
    "\\1\u0abe\u0aa1",
    "([\u0a95-\u0ab9])\u001daaN",
    "\\1\u0abe\u0aa3",
    "([\u0a95-\u0ab9])\u001daat",
    "\\1\u0abe\u0aa4",
    "([\u0a95-\u0ab9])\u001daad",
    "\\1\u0abe\u0aa6",
    "([\u0a95-\u0ab9])\u001daan",
    "\\1\u0abe\u0aa8",
    "([\u0a95-\u0ab9])\u001daap",
    "\\1\u0abe\u0aaa",
    "([\u0a95-\u0ab9])\u001daab",
    "\\1\u0abe\u0aac",
    "([\u0a95-\u0ab9])\u001daam",
    "\\1\u0abe\u0aae",
    "([\u0a95-\u0ab9])\u001daar",
    "\\1\u0abe\u0ab0",
    "([\u0a95-\u0ab9])\u001daal",
    "\\1\u0abe\u0ab2",
    "([\u0a95-\u0ab9])\u001daaL",
    "\\1\u0abe\u0ab3",
    "([\u0a95-\u0ab9])\u001daav",
    "\\1\u0abe\u0ab5",
    "([\u0a95-\u0ab9])\u001daaw",
    "\\1\u0abe\u0ab5",
    "([\u0a95-\u0ab9])\u001daaS",
    "\\1\u0abe\u0ab6",
    "([\u0a95-\u0ab9])\u001daas",
    "\\1\u0abe\u0ab8",
    "([\u0a95-\u0ab9])\u001daah",
    "\\1\u0abe\u0ab9",
    "([\u0a95-\u0ab9])\u001daaR",
    "\\1\u0abe\u0ab0\u0abc",
    "([\u0a95-\u0ab9])\u001daaq",
    "\\1\u0abe\u0a95\u0abc",
    "([\u0a95-\u0ab9])\u001daaKh",
    "\\1\u0abe\u0a96\u0abc",
    "([\u0a95-\u0ab9])\u001daaG",
    "\\1\u0abe\u0a97\u0abc",
    "([\u0a95-\u0ab9])\u001daaz",
    "\\1\u0abe\u0a9c\u0abc",
    "([\u0a95-\u0ab9])\u001daaJ",
    "\\1\u0abe\u0a9c\u0abc",
    "([\u0a95-\u0ab9])\u001daa.D",
    "\\1\u0abe\u0aa1\u0abc",
    "([\u0a95-\u0ab9])\u001daaf",
    "\\1\u0abe\u0aab\u0abc",
    "([\u0a95-\u0ab9])\u001daay",
    "\\1\u0abe\u0aaf\u0abc",
    "([\u0a95-\u0ab9])\u001daax",
    "\\1\u0abe\u0a95\u0acd\u0ab7"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune =
    "a|aa|ac|aaC|aac|a\\.|aK|aC|aaK|aS|aaS|aa~|aa\\.|a~";

}  // namespace gu_phone
