// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/hi_inscript.h"

namespace hi_inscript {

const char* kId = "hi_inscript";
bool kIs102 = false;

const char* kNormal[] = {
    "\u094a",  // BackQuote      : ॊ
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
    "\u0943",  // Equal          : ृ
    "\u094c",  // KeyQ           : ौ
    "\u0948",  // KeyW           : ै
    "\u093e",  // KeyE           : ा
    "\u0940",  // KeyR           : ी
    "\u0942",  // KeyT           : ू
    "\u092c",  // KeyY           : ब
    "\u0939",  // KeyU           : ह
    "\u0917",  // KeyI           : ग
    "\u0926",  // KeyO           : द
    "\u091c",  // KeyP           : ज
    "\u0921",  // BracketLeft    : ड
    "\u093c",  // BracketRight   : ़
    "\u0949",  // Backslash      : ॉ
    "\u094b",  // KeyA           : ो
    "\u0947",  // KeyS           : े
    "\u094d",  // KeyD           : ्
    "\u093f",  // KeyF           : ि
    "\u0941",  // KeyG           : ु
    "\u092a",  // KeyH           : प
    "\u0930",  // KeyJ           : र
    "\u0915",  // KeyK           : क
    "\u0924",  // KeyL           : त
    "\u091a",  // Semicolon      : च
    "\u091f",  // Quote          : ट
    "\u0946",  // KeyZ           : ॆ
    "\u0902",  // KeyX           : ं
    "\u092e",  // KeyC           : म
    "\u0928",  // KeyV           : न
    "\u0935",  // KeyB           : व
    "\u0932",  // KeyN           : ल
    "\u0938",  // KeyM           : स
    ",",       // Comma
    ".",       // Period
    "\u092f",  // Slash          : य
    "\u0020",  // Space
};

const char* kShift[] = {
    "\u0912",              // BackQuote      : ऒ
    "\u090d",              // Digit1         : ऍ
    "\u0945",              // Digit2         : ॅ
    "\u094d\u0930",        // Digit3         : ्र
    "\u0930\u094d",        // Digit4         : र्
    "\u091c\u094d\u091e",  // Digit5         : ज्ञ
    "\u0924\u094d\u0930",  // Digit6         : त्र
    "\u0915\u094d\u0937",  // Digit7         : क्ष
    "\u0936\u094d\u0930",  // Digit8         : श्र
    "(",                   // Digit9
    ")",                   // Digit0
    "\u0903",              // Minus          : ः
    "\u090b",              // Equal          : ऋ
    "\u0914",              // KeyQ           : औ
    "\u0910",              // KeyW           : ऐ
    "\u0906",              // KeyE           : आ
    "\u0908",              // KeyR           : ई
    "\u090a",              // KeyT           : ऊ
    "\u092d",              // KeyY           : भ
    "\u0919",              // KeyU           : ङ
    "\u0918",              // KeyI           : घ
    "\u0927",              // KeyO           : ध
    "\u091d",              // KeyP           : झ
    "\u0922",              // BracketLeft    : ढ
    "\u091e",              // BracketRight   : ञ
    "\u0911",              // Backslash      : ऑ
    "\u0913",              // KeyA           : ओ
    "\u090f",              // KeyS           : ए
    "\u0905",              // KeyD           : अ
    "\u0907",              // KeyF           : इ
    "\u0909",              // KeyG           : उ
    "\u092b",              // KeyH           : फ
    "\u0931",              // KeyJ           : ऱ
    "\u0916",              // KeyK           : ख
    "\u0925",              // KeyL           : थ
    "\u091b",              // Semicolon      : छ
    "\u0920",              // Quote          : ठ
    "\u090e",              // KeyZ           : ऎ
    "\u0901",              // KeyX           : ँ
    "\u0923",              // KeyC           : ण
    "\u0929",              // KeyV           : ऩ
    "\u0934",              // KeyB           : ऴ
    "\u0933",              // KeyN           : ळ
    "\u0936",              // KeyM           : श
    "\u0937",              // Comma          : ष
    "\u0964",              // Period         : ।
    "\u095f",              // Slash          : य़ = <U+092F, U+093C>
    "\u0020",              // Space
};

// For some keys, AltGr+Key combinations do not work when they're 'hijacked' by
// OS shortcuts (Alt+Key). For instance, AltGr+F is treated as Alt+F and
// the file browser opens instead of producing U+0962.
// See crbug.com/1354988 (b/243243592)
const char* kAltGr[] = {
    "",        // BackQuote
    "\u200d",  // Digit1         : ZWJ
    "\u200c",  // Digit2         : ZWNJ
    "",        // Digit3
    "\u20b9",  // Digit4         : ₹(Rupee sign)
    "",        // Digit5
    "",        // Digit6
    "",        // Digit7
    "",        // Digit8
    "",        // Digit9
    "",        // Digit0
    "",        // Minus
    "\u0944",  // Equal          : ॄ
    "",        // KeyQ
    "",        // KeyW
    "",        // KeyE
    "\u0963",  // KeyR           : ॣ
    "",        // KeyT
    "",        // KeyY
    "",        // KeyU
    "\u095a",  // KeyI           : ग़ = <U+0917, U+093C>
    "",        // KeyO
    "\u095b",  // KeyP           : ज़ = <U+091C, U+093C>
    "\u095c",  // BracketLeft    : ड़ = <U+0921, U+093C>
    "",        // BracketRight
    "",        // Backslash
    "",        // KeyA
    "",        // KeyS
    "",        // KeyD
    "\u0962",  // KeyF           : ॢ
    "",        // KeyG
    "",        // KeyH
    "",        // KeyJ
    "\u0958",  // KeyK           : क़ = <U+0915, U+093C>
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
    "\u0970",  // Comma          : ॰
    "\u0965",  // Period         : ॥
    "",        // Slash
    "",        // Space
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
    "\u0960",  // Equal          : ॠ
    "",        // KeyQ
    "",        // KeyW
    "",        // KeyE
    "\u0961",  // KeyR           : ॡ
    "",        // KeyT
    "",        // KeyY
    "",        // KeyU
    "",        // KeyI
    "",        // KeyO
    "",        // KeyP
    "\u095d",  // BracketLeft    : ढ़ = <U+0922, U+093C>
    "",        // BracketRight
    "",        // Backslash
    "",        // KeyA
    "",        // KeyS
    "",        // KeyD
    "\u090c",  // KeyF           : ऌ
    "",        // KeyG
    "\u095e",  // KeyH           : फ़ = <U+092B, U+093C>
    "",        // KeyJ
    "\u0959",  // KeyK           : ख़ = <U+0916, U+093C>
    "",        // KeyL
    "",        // Semicolon
    "",        // Quote
    "",        // KeyZ
    "\u0950",  // KeyX           : ॐ
    "",        // KeyC
    "",        // KeyV
    "",        // KeyB
    "",        // KeyN
    "",        // KeyM
    "",        // Comma
    "\u093d",  // Period         : ऽ
    "",        // Slash
    "",        // Space
};

// Capslock is identical to Normal (no modifier state) except that it
// is used to type Devanagari digits, U+0966 - U+096f.
const char* kCapslock[] = {
    "\u094a",  // BackQuote      : ॊ
    "\u0967",  // Digit1         : १
    "\u0968",  // Digit2         : २
    "\u0969",  // Digit3         : ३
    "\u096a",  // Digit4         : ४
    "\u096b",  // Digit5         : ५
    "\u096c",  // Digit6         : ६
    "\u096d",  // Digit7         : ७
    "\u096e",  // Digit8         : ८
    "\u096f",  // Digit9         : ९
    "\u0966",  // Digit0         : ०
    "-",       // Minus
    "\u0943",  // Equal          : ृ
    "\u094c",  // KeyQ           : ौ
    "\u0948",  // KeyW           : ै
    "\u093e",  // KeyE           : ा
    "\u0940",  // KeyR           : ी
    "\u0942",  // KeyT           : ू
    "\u092c",  // KeyY           : ब
    "\u0939",  // KeyU           : ह
    "\u0917",  // KeyI           : ग
    "\u0926",  // KeyO           : द
    "\u091c",  // KeyP           : ज
    "\u0921",  // BracketLeft    : ड
    "\u093c",  // BracketRight   : ़
    "\u0949",  // Backslash      : ॉ
    "\u094b",  // KeyA           : ो
    "\u0947",  // KeyS           : े
    "\u094d",  // KeyD           : ्
    "\u093f",  // KeyF           : ि
    "\u0941",  // KeyG           : ु
    "\u092a",  // KeyH           : प
    "\u0930",  // KeyJ           : र
    "\u0915",  // KeyK           : क
    "\u0924",  // KeyL           : त
    "\u091a",  // Semicolon      : च
    "\u091f",  // Quote          : ट
    "\u0946",  // KeyZ           : ॆ
    "\u0902",  // KeyX           : ं
    "\u092e",  // KeyC           : म
    "\u0928",  // KeyV           : न
    "\u0935",  // KeyB           : व
    "\u0932",  // KeyN           : ल
    "\u0938",  // KeyM           : स
    ",",       // Comma
    ".",       // Period
    "\u092f",  // Slash          : य
    "\u0020",  // Space
};

// Note that Capslock + {Shift, AltGr, Shift+AltGr} are identical to those
// without Capslock.
const char** kKeyMap[8] = {kNormal,   kShift, kAltGr, kShiftAltGr,
                           kCapslock, kShift, kAltGr, kShiftAltGr};

}  // namespace hi_inscript
