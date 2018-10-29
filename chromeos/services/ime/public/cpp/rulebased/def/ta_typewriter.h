// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_TYPEWRITER_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_TYPEWRITER_H_

const wchar_t* key_map_ta_typewriter[] = {
    // Row #1
    L"\u0b831234567890/="
    // Row #2
    L"{{\u0ba3\u0bc1}}\u0bb1\u0ba8\u0b9a\u0bb5\u0bb2\u0bb0\u0bc8{{\u0b9f\u0bbf}"
    L"}\u0bbf\u0bc1\u0bb9{{\u0b95\u0bcd\u0bb7}}"
    // Row #3
    L"\u0baf\u0bb3\u0ba9\u0b95\u0baa\u0bbe\u0ba4\u0bae\u0b9f\u0bcd\u0b99"
    // Row #4
    L"\u0ba3\u0b92\u0b89\u0b8e\u0bc6\u0bc7\u0b85\u0b87,."
    // Row #5
    L"\u0020",
    // Row #1
    L"'\u0bb8\"%\u0b9c\u0bb6\u0bb7{{}}{{}}(){{\u0bb8\u0bcd\u0bb0\u0bc0}}+"
    // Row #2
    L"{{}}{{\u0bb1\u0bc1}}{{\u0ba8\u0bc1}}{{\u0b9a\u0bc1}}{{\u0b95\u0bc2}}{{"
    L"\u0bb2\u0bc1}}{{\u0bb0\u0bc1}}\u0b90{{\u0b9f\u0bc0}}"
    L"\u0bc0\u0bc2\u0bcc\u0bf8"
    // Row #3
    L"{{}}{{\u0bb3\u0bc1}}{{\u0ba9\u0bc1}}{{\u0b95\u0bc1}}{{\u0bb4\u0bc1}}"
    L"\u0bb4{{\u0ba4\u0bc1}}{{\u0bae\u0bc1}}{{\u0b9f\u0bc1}}\\\u0b9e"
    // Row #4
    L"\u0bb7\u0b93\u0b8a\u0b8f{{\u0b95\u0bcd\u0bb7}}{{\u0b9a\u0bc2}}\u0b86"
    L"\u0b88?-"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_ta_typewriter[8]{0, 1, 0, 1, 1, 0, 1, 0};
const char* id_ta_typewriter = "ta_typewriter";
const bool is_102_ta_typewriter = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_TYPEWRITER_H_
