// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_AR_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_AR_H_

const wchar_t* key_map_ar[] = {
    // Row #1
    L"\u0630\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u0669\u0660-="
    // Row #2
    L"\u0636\u0635\u062b\u0642\u0641\u063a\u0639\u0647\u062e\u062d\u062c\u062f"
    L"\\"
    // Row #3
    L"\u0634\u0633\u064a\u0628\u0644\u0627\u062a\u0646\u0645\u0643\u0637"
    // Row #4
    L"\u0626\u0621\u0624\u0631{{\u0644\u0627}}\u0649\u0629\u0648\u0632\u0638"
    // Row #5
    L"\u0020",
    // Row #1
    L"\u0651!\"#$%^&*)(_+"
    // Row #2
    L"\u064e\u064b\u064f\u064c{{\u0644\u0625}}\u0625\u2018\u00f7\u00d7\u061b<>|"
    // Row #3
    L"\u0650\u064d][{{\u0644\u0623}}\u0623\u0640\u060c/:@"
    // Row #4
    L"~\u0652}{((\u0644\u0622))\u0622\u2019,.\u061f"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_ar[8]{0, 1, 0, 1, 0, 1, 0, 1};
const char* id_ar = "ar";
const bool is_102_ar = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_AR_H_
