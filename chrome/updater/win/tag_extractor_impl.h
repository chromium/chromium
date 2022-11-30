// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TAG_EXTRACTOR_IMPL_H_
#define CHROME_UPDATER_WIN_TAG_EXTRACTOR_IMPL_H_

#include <stdint.h>

#include <vector>

#include "chrome/updater/win/tag_extractor.h"

namespace updater {

using BinaryConstIt = std::vector<uint8_t>::const_iterator;

// These functions are available for unit testing.

// Advances the iterator by |distance| and makes sure that it remains valid,
// else returns |end|.
BinaryConstIt AdvanceIt(BinaryConstIt it, size_t distance, BinaryConstIt end);

// Checks that the range [it, it + size) is found within the binary. |size| must
// be > 0.
bool CheckRange(BinaryConstIt it, size_t size, BinaryConstIt end);

// Included in this header for fuzz testing.
std::string ExtractTagFromBuffer(const std::vector<uint8_t>& binary,
                                 TagEncoding encoding);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TAG_EXTRACTOR_IMPL_H_
