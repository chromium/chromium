// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_KM_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_KM_H_

const wchar_t* key_map_km[] = {
    // Row #1
    L"\u00ab\u17e1\u17e2\u17e3\u17e4\u17e5\u17e6\u17e7\u17e8\u17e9\u17e0\u17a5"
    L"\u17b2"
    // Row #2
    L"\u1786\u17b9\u17c1\u179a\u178f\u1799\u17bb\u17b7\u17c4\u1795\u17c0\u17aa"
    L"\u17ae"
    // Row #3
    L"\u17b6\u179f\u178a\u1790\u1784\u17a0\u17d2\u1780\u179b\u17be\u17cb"
    // Row #4
    L"\u178b\u1781\u1785\u179c\u1794\u1793\u1798{{\u17bb\u17c6}}\u17d4\u17ca"
    // Row #5
    L"\u200b",
    // Row #1
    L"\u00bb!\u17d7\"\u17db%\u17cd\u17d0\u17cf()\u17cc="
    // Row #2
    L"\u1788\u17ba\u17c2\u17ac\u1791\u17bd\u17bc\u17b8\u17c5\u1797\u17bf\u17a7"
    L"\u17ad"
    // Row #3
    L"{{\u17b6\u17c6}}\u17c3\u178c\u1792\u17a2\u17c7\u1789\u1782\u17a1{{\u17c4"
    L"\u17c7}}\u17c9"
    // Row #4
    L"\u178d\u1783\u1787{{\u17c1\u17c7}}\u1796\u178e\u17c6{{\u17bb\u17c7}}"
    L"\u17d5?"
    // Row #5
    L"\u0020",
    // Row #1
    L"\u200d\u200c@\u17d1$\u20ac\u17d9\u17da*{}\u00d7\u17ce"
    // Row #2
    L"\u17dc\u17dd\u17af\u17ab\u17a8{{}}{{}}\u17a6\u17b1\u17b0\u17a9\u17b3\\"
    // Row #3
    L"+-{{}}{{}}{{}}{{}}{{}}\u179d{{}}\u17d6\u17c8"
    // Row #4
    L"{{}}{{}}{{}}{{}}\u179e{{}}{{}},./"
    // Row #5
    L"\u0020",
    // Row #1
    L"{{}}\u17f1\u17f2\u17f3\u17f4\u17f5\u17f6\u17f7\u17f8\u17f9\u17f0{{}}{{}}"
    // Row #2
    L"\u19e0\u19e1\u19e2\u19e3\u19e4\u19e5\u19e6\u19e7\u19e8\u19e9\u19ea\u19eb{"
    L"{}}"
    // Row #3
    L"\u19ec\u19ed\u19ee\u19ef\u19f0\u19f1\u19f2\u19f3\u19f4\u19f5\u19f6"
    // Row #4
    L"\u19f7\u19f8\u19f9\u19fa\u19fb\u19fc\u19fd\u19fe\u19ff{{}}"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_km[8]{0, 1, 2, 3, 1, 0, 3, 2};
const char* id_km = "km";
const bool is_102_km = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_KM_H_
