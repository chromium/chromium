// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ne_inscript.h"

namespace ne_inscript {

const char* kId = "ne_inscript";
bool kIs102 = false;
const char* kNormal[] = {
    "\u091e",              // BackQuote
    "\u0967",              // Digit1
    "\u0968",              // Digit2
    "\u0969",              // Digit3
    "\u096a",              // Digit4
    "\u096b",              // Digit5
    "\u096c",              // Digit6
    "\u096d",              // Digit7
    "\u096e",              // Digit8
    "\u096f",              // Digit9
    "\u0966",              // Digit0
    "\u0914",              // Minus
    "\u200d",              // Equal
    "\u0924\u094d\u0930",  // KeyQ
    "\u0927",              // KeyW
    "\u092d",              // KeyE
    "\u091a",              // KeyR
    "\u0924",              // KeyT
    "\u0925",              // KeyY
    "\u0917",              // KeyU
    "\u0937",              // KeyI
    "\u092f",              // KeyO
    "\u0909",              // KeyP
    "\u0930\u094d",        // BracketLeft
    "\u0947",              // BracketRight
    "\u094d",              // Backslash
    "\u092c",              // KeyA
    "\u0915",              // KeyS
    "\u092e",              // KeyD
    "\u093e",              // KeyF
    "\u0928",              // KeyG
    "\u091c",              // KeyH
    "\u0935",              // KeyJ
    "\u092a",              // KeyK
    "\u093f",              // KeyL
    "\u0938",              // Semicolon
    "\u0941",              // Quote
    "\u0936",              // KeyZ
    "\u0939",              // KeyX
    "\u0905",              // KeyC
    "\u0916",              // KeyV
    "\u0926",              // KeyB
    "\u0932",              // KeyN
    "\u0903",              // KeyM
    "\u093d",              // Comma
    "\u0964",              // Period
    "\u0930",              // Slash
    "\u0020",              // Space
};
const char* kShift[] = {
    "\u0965",              // BackQuote
    "\u091c\u094d\u091e",  // Digit1
    "\u0908",              // Digit2
    "\u0918",              // Digit3
    "$",                   // Digit4
    "\u091b",              // Digit5
    "\u091f",              // Digit6
    "\u0920",              // Digit7
    "\u0921",              // Digit8
    "\u0922",              // Digit9
    "\u0923",              // Digit0
    "\u0913",              // Minus
    "\u200c",              // Equal
    "\u0924\u094d\u0924",  // KeyQ
    "\u0921\u094d\u0922",  // KeyW
    "\u0910",              // KeyE
    "\u0926\u094d\u0935",  // KeyR
    "\u091f\u094d\u091f",  // KeyT
    "\u0920\u094d\u0920",  // KeyY
    "\u090a",              // KeyU
    "\u0915\u094d\u0937",  // KeyI
    "\u0907",              // KeyO
    "\u090f",              // KeyP
    "\u0943",              // BracketLeft
    "\u0948",              // BracketRight
    "\u0902",              // Backslash
    "\u0906",              // KeyA
    "\u0919\u094d\u0915",  // KeyS
    "\u0919\u094d\u0917",  // KeyD
    "\u0901",              // KeyF
    "\u0926\u094d\u0926",  // KeyG
    "\u091d",              // KeyH
    "\u094b",              // KeyJ
    "\u092b",              // KeyK
    "\u0940",              // KeyL
    "\u091f\u094d\u0920",  // Semicolon
    "\u0942",              // Quote
    "\u0915\u094d\u0915",  // KeyZ
    "\u0939\u094d\u092e",  // KeyX
    "\u090b",              // KeyC
    "\u0950",              // KeyV
    "\u094c",              // KeyB
    "\u0926\u094d\u092f",  // KeyN
    "\u0921\u094d\u0921",  // KeyM
    "\u0919",              // Comma
    "\u0936\u094d\u0930",  // Period
    "\u0930\u0941",        // Slash
    "\u0020",              // Space
};
const char* kCapslock[] = {
    "\u091e",              // BackQuote
    "\u0967",              // Digit1
    "\u0968",              // Digit2
    "\u0969",              // Digit3
    "\u096a",              // Digit4
    "\u096b",              // Digit5
    "\u096c",              // Digit6
    "\u096d",              // Digit7
    "\u096e",              // Digit8
    "\u096f",              // Digit9
    "\u0966",              // Digit0
    "\u0914",              // Minus
    "\u200d",              // Equal
    "\u0924\u094d\u0930",  // KeyQ
    "\u0927",              // KeyW
    "\u092d",              // KeyE
    "\u091a",              // KeyR
    "\u0924",              // KeyT
    "\u0925",              // KeyY
    "\u0917",              // KeyU
    "\u0937",              // KeyI
    "\u092f",              // KeyO
    "\u0909",              // KeyP
    "\u0930\u094d",        // BracketLeft
    "\u0947",              // BracketRight
    "\u094d",              // Backslash
    "\u092c",              // KeyA
    "\u0915",              // KeyS
    "\u092e",              // KeyD
    "\u093e",              // KeyF
    "\u0928",              // KeyG
    "\u091c",              // KeyH
    "\u0935",              // KeyJ
    "\u092a",              // KeyK
    "\u093f",              // KeyL
    "\u0938",              // Semicolon
    "\u0941",              // Quote
    "\u0936",              // KeyZ
    "\u0939",              // KeyX
    "\u0905",              // KeyC
    "\u0916",              // KeyV
    "\u0926",              // KeyB
    "\u0932",              // KeyN
    "\u0903",              // KeyM
    "\u093d",              // Comma
    "\u0964",              // Period
    "\u0930",              // Slash
    "\u0020",              // Space
};
const char* kShiftCapslock[] = {
    "\u0965",              // BackQuote
    "\u091c\u094d\u091e",  // Digit1
    "\u0908",              // Digit2
    "\u0918",              // Digit3
    "$",                   // Digit4
    "\u091b",              // Digit5
    "\u091f",              // Digit6
    "\u0920",              // Digit7
    "\u0921",              // Digit8
    "\u0922",              // Digit9
    "\u0923",              // Digit0
    "\u0913",              // Minus
    "\u200c",              // Equal
    "\u0924\u094d\u0924",  // KeyQ
    "\u0921\u094d\u0922",  // KeyW
    "\u0910",              // KeyE
    "\u0926\u094d\u0935",  // KeyR
    "\u091f\u094d\u091f",  // KeyT
    "\u0920\u094d\u0920",  // KeyY
    "\u090a",              // KeyU
    "\u0915\u094d\u0937",  // KeyI
    "\u0907",              // KeyO
    "\u090f",              // KeyP
    "\u0943",              // BracketLeft
    "\u0948",              // BracketRight
    "\u0902",              // Backslash
    "\u0906",              // KeyA
    "\u0919\u094d\u0915",  // KeyS
    "\u0919\u094d\u0917",  // KeyD
    "\u0901",              // KeyF
    "\u0926\u094d\u0926",  // KeyG
    "\u091d",              // KeyH
    "\u094b",              // KeyJ
    "\u092b",              // KeyK
    "\u0940",              // KeyL
    "\u091f\u094d\u0920",  // Semicolon
    "\u0942",              // Quote
    "\u0915\u094d\u0915",  // KeyZ
    "\u0939\u094d\u092e",  // KeyX
    "\u090b",              // KeyC
    "\u0950",              // KeyV
    "\u094c",              // KeyB
    "\u0926\u094d\u092f",  // KeyN
    "\u0921\u094d\u0921",  // KeyM
    "\u0919",              // Comma
    "\u0936\u094d\u0930",  // Period
    "\u0930\u0941",        // Slash
    "\u0020",              // Space
};
const char** kKeyMap[8] = {kNormal,   kShift,        kNormal,
                           kShift,    kCapslock,     kShiftCapslock,
                           kCapslock, kShiftCapslock};

}  // namespace ne_inscript
