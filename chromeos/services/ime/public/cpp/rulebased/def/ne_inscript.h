// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_NE_INSCRIPT_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_NE_INSCRIPT_H_

const wchar_t* key_map_ne_inscript[] = {
    // Row #1
    L"\u091e\u0967\u0968\u0969\u096a\u096b\u096c\u096d\u096e\u096f\u0966\u0914"
    L"\u200d"
    // Row #2
    L"{{\u0924\u094d\u0930}}\u0927\u092d\u091a\u0924\u0925\u0917\u0937\u092f"
    L"\u0909{{\u0930\u094d}}\u0947\u094d"
    // Row #3
    L"\u092c\u0915\u092e\u093e\u0928\u091c\u0935\u092a\u093f\u0938\u0941"
    // Row #4
    L"\u0936\u0939\u0905\u0916\u0926\u0932\u0903\u093d\u0964\u0930"
    // Row #5
    L"\u0020",
    // Row #1
    L"\u0965{{\u091c\u094d\u091e}}\u0908\u0918$\u091b\u091f\u0920\u0921\u0922"
    L"\u0923\u0913\u200c"
    // Row #2
    L"{{\u0924\u094d\u0924}}{{\u0921\u094d\u0922}}\u0910{{\u0926\u094d\u0935}}{"
    L"{\u091f\u094d\u091f}}{{\u0920\u094d\u0920}}\u090a{{\u0915\u094d\u0937}}"
    L"\u0907"
    L"\u090f\u0943\u0948\u0902"
    // Row #3
    L"\u0906{{\u0919\u094d\u0915}}{{\u0919\u094d\u0917}}\u0901{{\u0926\u094d"
    L"\u0926}}\u091d\u094b\u092b\u0940{{\u091f\u094d\u0920}}\u0942"
    // Row #4
    L"{{\u0915\u094d\u0915}}{{\u0939\u094d\u092e}}\u090b\u0950\u094c{{\u0926"
    L"\u094d\u092f}}{{\u0921\u094d\u0921}}\u0919{{\u0936\u094d\u0930}}{{"
    L"\u0930\u0941}}"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_ne_inscript[8]{0, 1, 0, 1, 0, 1, 0, 1};
const char* id_ne_inscript = "ne_inscript";
const bool is_102_ne_inscript = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_NE_INSCRIPT_H_
