// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_LO_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_LO_H_

const wchar_t* key_map_lo[] = {
    // Row #1
    L"*\u0ea2\u0e9f\u0ec2\u0e96\u0eb8\u0eb9\u0e84\u0e95\u0e88\u0e82\u0e8a\u0ecd"
    // Row #2
    L"\u0ebb\u0ec4\u0eb3\u0e9e\u0eb0\u0eb4\u0eb5\u0eae\u0e99\u0e8d\u0e9a\u0ea5"
    L"\u201c"
    // Row #3
    L"\u0eb1\u0eab\u0e81\u0e94\u0ec0\u0ec9\u0ec8\u0eb2\u0eaa\u0ea7\u0e87"
    // Row #4
    L"\u0e9c\u0e9b\u0ec1\u0ead\u0eb6\u0eb7\u0e97\u0ea1\u0ec3\u0e9d"
    // Row #5
    L"\u0020",
    // Row #1
    L"/1234\u0ecc\u0ebc56789{{\u0ecd\u0ec8}}"
    // Row #2
    L"{{\u0ebb\u0ec9}}0{{\u0eb3\u0ec9}}_+{{\u0eb4\u0ec9}}{{\u0eb5\u0ec9}}\u0ea3"
    L"\u0edc\u0ebd-{{\u0eab\u0ebc}}\u201d"
    // Row #3
    L"{{\u0eb1\u0ec9}};.,:\u0eca\u0ecb!?%="
    // Row #4
    L"\u20ad(\u0eafx{{\u0eb6\u0ec9}}{{\u0eb7\u0ec9}}\u0ec6\u0edd$)"
    // Row #5
    L"\u0020",
    // Row #1
    L"{{}}\u0ed1\u0ed2\u0ed3\u0ed4\u0ed5\u0ed6\u0ed7\u0ed8\u0ed9\u0ed0{{}}{{}}"
    // Row #2
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #3
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #4
    L"{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}{{}}"
    // Row #5
    L"\u0020"};

const uint8_t key_map_index_lo[8]{0, 1, 2, 1, 0, 1, 2, 1};
const char* id_lo = "lo";
const bool is_102_lo = false;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_LO_H_
