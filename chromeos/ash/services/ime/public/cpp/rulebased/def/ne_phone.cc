// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ne_phone.h"

namespace ne_phone {

const char* kId = "ne_phone";
bool kIs102 = false;
const char* kNormal[] = {
    "\u093c",  // BackQuote
    "\u0967",  // Digit1
    "\u0968",  // Digit2
    "\u0969",  // Digit3
    "\u096a",  // Digit4
    "\u096b",  // Digit5
    "\u096c",  // Digit6
    "\u096d",  // Digit7
    "\u096e",  // Digit8
    "\u096f",  // Digit9
    "\u0966",  // Digit0
    "-",       // Minus
    "\u200d",  // Equal
    "\u091f",  // KeyQ
    "\u094c",  // KeyW
    "\u0947",  // KeyE
    "\u0930",  // KeyR
    "\u0924",  // KeyT
    "\u092f",  // KeyY
    "\u0941",  // KeyU
    "\u093f",  // KeyI
    "\u094b",  // KeyO
    "\u092a",  // KeyP
    "\u0907",  // BracketLeft
    "\u090f",  // BracketRight
    "\u0950",  // Backslash
    "\u093e",  // KeyA
    "\u0938",  // KeyS
    "\u0926",  // KeyD
    "\u0909",  // KeyF
    "\u0917",  // KeyG
    "\u0939",  // KeyH
    "\u091c",  // KeyJ
    "\u0915",  // KeyK
    "\u0932",  // KeyL
    ";",       // Semicolon
    "'",       // Quote
    "\u0937",  // KeyZ
    "\u0921",  // KeyX
    "\u091a",  // KeyC
    "\u0935",  // KeyV
    "\u092c",  // KeyB
    "\u0928",  // KeyN
    "\u092e",  // KeyM
    ",",       // Comma
    "\u0964",  // Period
    "\u094d",  // Slash
    "\u0020",  // Space
};
const char* kShift[] = {
    "\u093d",         // BackQuote
    "!",              // Digit1
    "@",              // Digit2
    "#",              // Digit3
    "\u0930\u0941.",  // Digit4
    "%",              // Digit5
    "^",              // Digit6
    "&",              // Digit7
    "*",              // Digit8
    "(",              // Digit9
    ")",              // Digit0
    "_",              // Minus
    "\u200c",         // Equal
    "\u0920",         // KeyQ
    "\u0914",         // KeyW
    "\u0948",         // KeyE
    "\u0943",         // KeyR
    "\u0925",         // KeyT
    "\u091e",         // KeyY
    "\u0942",         // KeyU
    "\u0940",         // KeyI
    "\u0913",         // KeyO
    "\u092b",         // KeyP
    "\u0908",         // BracketLeft
    "\u0910",         // BracketRight
    "\u0903",         // Backslash
    "\u0906",         // KeyA
    "\u0936",         // KeyS
    "\u0927",         // KeyD
    "\u090a",         // KeyF
    "\u0918",         // KeyG
    "\u0905",         // KeyH
    "\u091d",         // KeyJ
    "\u0916",         // KeyK
    "\u0965",         // KeyL
    ":",              // Semicolon
    "\"",             // Quote
    "\u090b",         // KeyZ
    "\u0922",         // KeyX
    "\u091b",         // KeyC
    "\u0901",         // KeyV
    "\u092d",         // KeyB
    "\u0923",         // KeyN
    "\u0902",         // KeyM
    "\u0919",         // Comma
    ".",              // Period
    "?",              // Slash
    "\u0020",         // Space
};
const char* kCapslock[] = {
    "\u093c",  // BackQuote
    "\u0967",  // Digit1
    "\u0968",  // Digit2
    "\u0969",  // Digit3
    "\u096a",  // Digit4
    "\u096b",  // Digit5
    "\u096c",  // Digit6
    "\u096d",  // Digit7
    "\u096e",  // Digit8
    "\u096f",  // Digit9
    "\u0966",  // Digit0
    "-",       // Minus
    "\u200d",  // Equal
    "\u091f",  // KeyQ
    "\u094c",  // KeyW
    "\u0947",  // KeyE
    "\u0930",  // KeyR
    "\u0924",  // KeyT
    "\u092f",  // KeyY
    "\u0941",  // KeyU
    "\u093f",  // KeyI
    "\u094b",  // KeyO
    "\u092a",  // KeyP
    "\u0907",  // BracketLeft
    "\u090f",  // BracketRight
    "\u0950",  // Backslash
    "\u093e",  // KeyA
    "\u0938",  // KeyS
    "\u0926",  // KeyD
    "\u0909",  // KeyF
    "\u0917",  // KeyG
    "\u0939",  // KeyH
    "\u091c",  // KeyJ
    "\u0915",  // KeyK
    "\u0932",  // KeyL
    ";",       // Semicolon
    "'",       // Quote
    "\u0937",  // KeyZ
    "\u0921",  // KeyX
    "\u091a",  // KeyC
    "\u0935",  // KeyV
    "\u092c",  // KeyB
    "\u0928",  // KeyN
    "\u092e",  // KeyM
    ",",       // Comma
    "\u0964",  // Period
    "\u094d",  // Slash
    "\u0020",  // Space
};
const char* kShiftCapslock[] = {
    "\u093d",         // BackQuote
    "!",              // Digit1
    "@",              // Digit2
    "#",              // Digit3
    "\u0930\u0941.",  // Digit4
    "%",              // Digit5
    "^",              // Digit6
    "&",              // Digit7
    "*",              // Digit8
    "(",              // Digit9
    ")",              // Digit0
    "_",              // Minus
    "\u200c",         // Equal
    "\u0920",         // KeyQ
    "\u0914",         // KeyW
    "\u0948",         // KeyE
    "\u0943",         // KeyR
    "\u0925",         // KeyT
    "\u091e",         // KeyY
    "\u0942",         // KeyU
    "\u0940",         // KeyI
    "\u0913",         // KeyO
    "\u092b",         // KeyP
    "\u0908",         // BracketLeft
    "\u0910",         // BracketRight
    "\u0903",         // Backslash
    "\u0906",         // KeyA
    "\u0936",         // KeyS
    "\u0927",         // KeyD
    "\u090a",         // KeyF
    "\u0918",         // KeyG
    "\u0905",         // KeyH
    "\u091d",         // KeyJ
    "\u0916",         // KeyK
    "\u0965",         // KeyL
    ":",              // Semicolon
    "\"",             // Quote
    "\u090b",         // KeyZ
    "\u0922",         // KeyX
    "\u091b",         // KeyC
    "\u0901",         // KeyV
    "\u092d",         // KeyB
    "\u0923",         // KeyN
    "\u0902",         // KeyM
    "\u0919",         // Comma
    ".",              // Period
    "?",              // Slash
    "\u0020",         // Space
};
const char** kKeyMap[8] = {kNormal,   kShift,        kNormal,
                           kShift,    kCapslock,     kShiftCapslock,
                           kCapslock, kShiftCapslock};

}  // namespace ne_phone
