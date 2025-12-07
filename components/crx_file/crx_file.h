// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_CRX_FILE_H_
#define COMPONENTS_CRX_FILE_CRX_FILE_H_

namespace crx_file {

// The magic string embedded in the header.
inline constexpr auto kCrxFileHeaderMagic =
    std::to_array<uint8_t>({'C', 'r', '2', '4'});
inline constexpr auto kCrxDiffFileHeaderMagic =
    std::to_array<uint8_t>({'C', 'r', 'O', 'D'});
inline constexpr auto kSignatureContext =
    std::to_array<uint8_t>({'C', 'R', 'X', '3', ' ', 'S', 'i', 'g', 'n', 'e',
                            'd', 'D', 'a', 't', 'a', 0});

}  // namespace crx_file

#endif  // COMPONENTS_CRX_FILE_CRX_FILE_H_
