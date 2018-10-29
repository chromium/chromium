// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_INSCRIPT_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_INSCRIPT_H_

const wchar_t* key_map_ta_inscript[] = {
    // Row #1
    L"\u0bca1234567890-\u0bf2"
    // Row #2
    L"\u0bcc\u0bc8\u0bbe\u0bc0\u0bc2\u0baa\u0bb9\u0b95\u0ba4\u0b9c\u0b9f\u0b9e"
    L"\u0b9f"
    // Row #3
    L"\u0bcb\u0bc7\u0bcd\u0bbf\u0bc1\u0baa\u0bb0\u0b95\u0ba4\u0b9a\u0b9f"
    // Row #4
    L"\u0bc6\u0b82\u0bae\u0ba8\u0bb5\u0bb2\u0bb8,.\u0baf"
    // Row #5
    L"\u0020",
    // Row #1
    L"\u0b92\u0b95\u0be8{{\u0bcd\u0bb0}}{{\u0bb0\u0bcd}}{{\u0b9c\u0bcd\u0b9e}}{"
    L"{\u0ba4\u0bcd\u0bb0}}{{\u0b95\u0bcd\u0bb7}}{{\u0bb6\u0bcd\u0bb0}}()"
    L"\u0b83{{}}"
    // Row #2
    L"\u0b94\u0b90\u0b86\u0b88\u0b8a{{}}\u0b99{{}}{{}}\u0b9a{{}}{{}}{{}}"
    // Row #3
    L"\u0b93\u0b8f\u0b85\u0b87\u0b89{{}}\u0bb1{{}}{{}}{{}}{{}}"
    // Row #4
    L"\u0b8e{{}}\u0ba3\u0ba9\u0bb4\u0bb3\u0bb6\u0bb7{{\u0bb8\u0bcd\u0bb0\u0bc0}"
    L"}{{}}"
    // Row #5
    L"\u0020",
    // Row #1
    L"{{}}\u0be7\u0be8\u0be9\u0bea\u0beb\u0bec\u0bed\u0bee\u0bef\u0be6{{}}"
    L"\u0bf2"
    // Row #2
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #3
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #4
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_ta_inscript[8]{0, 1, 2, 1, 0, 1, 0, 1};
const char* id_ta_inscript = "ta_inscript";
const bool is_102_ta_inscript = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_INSCRIPT_H_
