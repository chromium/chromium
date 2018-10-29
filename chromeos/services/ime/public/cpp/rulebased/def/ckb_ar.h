// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_CKB_AR_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_CKB_AR_H_

const wchar_t* key_map_ckb_ar[] = {
    // Row #1
    L"\u0698\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u0669\u0660-="
    // Row #2
    L"\u0686\u0635\u067e\u0642\u0641\u063a\u0639\u0647\u062e\u062d\u062c\u062f"
    L"\\"
    // Row #3
    L"\u0634\u0633\u06cc\u0628\u0644\u0627\u062a\u0646\u0645\u06a9\u06af"
    // Row #4
    L"\u0626\u0621\u06c6\u0631{{\u0644\u0627}}\u0649{{\u0647\u200c}}\u0648"
    L"\u0632/"
    // Row #5
    L"\u0020",
    // Row #1
    L"~!@#$%\u00bb\u00ab*)(_+"
    // Row #2
    L"\u0636}\u062b{\u06a4\u0625{{}}'\"\u061b><|"
    // Row #3
    L"][\u06ce{{}}\u06b5\u0623\u0640\u060c/:\u0637"
    // Row #4
    L"\u2904{{\u0648\u0648}}\u0624\u0695{{\u06b5\u0627}}\u0622\u0629,.\u061f"
    // Row #5
    L"\u200c"};

const uint8_t key_map_index_ckb_ar[8]{0, 1, 0, 1, 0, 1, 0, 0};
const char* id_ckb_ar = "ckb_ar";
const bool is_102_ckb_ar = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_CKB_AR_H_
