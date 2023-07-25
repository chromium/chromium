// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TAG_EXTRACTOR_IMPL_H_
#define CHROME_UPDATER_WIN_TAG_EXTRACTOR_IMPL_H_

#include <stdint.h>

#include <vector>

#include "chrome/updater/win/tag_extractor.h"

namespace updater {

// Included in this header for fuzz testing.
std::string ExtractTagFromBuffer(const std::vector<uint8_t>& binary,
                                 TagEncoding encoding);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TAG_EXTRACTOR_IMPL_H_
