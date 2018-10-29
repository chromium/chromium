// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_FA_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_FA_H_

const wchar_t* key_map_fa[] = {
    // Row #1
    L"\u200d\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9\u06f0-="
    // Row #2
    L"\u0636\u0635\u062b\u0642\u0641\u063a\u0639\u0647\u062e\u062d\u062c\u0686"
    L"\\"
    // Row #3
    L"\u0634\u0633\u06cc\u0628\u0644\u0627\u062a\u0646\u0645\u06a9\u06af"
    // Row #4
    L"\u0638\u0637\u0632\u0631\u0630\u062f\u067e\u0648./"
    // Row #5
    L"\u0020",
    // Row #1
    L"{{}}!\u066c\u066b{{\u0631\u06cc\u0627\u0644}}\u066a\u00d7\u060c*)(\u0640+"
    // Row #2
    L"\u0652\u064c\u064d\u064b\u064f\u0650\u064e\u0651][}{|"
    // Row #3
    L"\u0624\u0626\u064a\u0625\u0623\u0622\u0629\u00bb\u00ab:\u061b"
    // Row #4
    L"\u0643{{}}\u0698{{}}\u200c{{}}\u0621<>\u061f"
    // Row #5
    L"\u200c",
    // Row #1
    L"~`@#$%^&\u2022\u200e\u200f_\u2212"
    // Row #2
    L"\u00b0{{}}\u20ac{{}}{{}}{{}}{{}}\u202d\u202e\u202c\u202a\u202b\u2010"
    // Row #3
    L"{{}}{{}}\u0649{{}}{{}}\u0671{{}}\ufd3e\ufd3f;\""
    // Row #4
    L"\u00a0{{}}{{}}\u0656\u200d\u0655\u2026,'?"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_fa[8]{0, 1, 2, 1, 0, 1, 0, 1};
const char* id_fa = "fa";
const bool is_102_fa = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_FA_H_
