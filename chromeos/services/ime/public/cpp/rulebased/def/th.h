// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TH_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TH_H_

const wchar_t* key_map_th[] = {
    // Row #1
    L"_\u0e45/-\u0e20\u0e16\u0e38\u0e36\u0e04\u0e15\u0e08\u0e02\u0e0a"
    // Row #2
    L"\u0e46\u0e44\u0e33\u0e1e\u0e30\u0e31\u0e35\u0e23\u0e19\u0e22\u0e1a\u0e25"
    L"\u0e03"
    // Row #3
    L"\u0e1f\u0e2b\u0e01\u0e14\u0e40\u0e49\u0e48\u0e32\u0e2a\u0e27\u0e07"
    // Row #4
    L"\u0e1c\u0e1b\u0e41\u0e2d\u0e34\u0e37\u0e17\u0e21\u0e43\u0e1d"
    // Row #5
    L"\u0020",
    // Row #1
    L"%+\u0e51\u0e52\u0e53\u0e54\u0e39\u0e3f\u0e55\u0e56\u0e57\u0e58\u0e59"
    // Row #2
    L"\u0e50\"\u0e0e\u0e11\u0e18\u0e4d\u0e4a\u0e13\u0e2f\u0e0d\u0e10,\u0e05"
    // Row #3
    L"\u0e24\u0e06\u0e0f\u0e42\u0e0c\u0e47\u0e4b\u0e29\u0e28\u0e0b."
    // Row #4
    L"()\u0e09\u0e2e\u0e3a\u0e4c?\u0e12\u0e2c\u0e26"
    // Row #5
    L"\u0020",
    // Row #1
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #2
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}%\u0e51+"
    // Row #3
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #4
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_th[8]{0, 1, 2, 1, 1, 0, 2, 0};
const char* id_th = "th";
const bool is_102_th = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TH_H_
