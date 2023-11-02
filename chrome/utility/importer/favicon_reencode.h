// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_FAVICON_REENCODE_H_
#define CHROME_UTILITY_IMPORTER_FAVICON_REENCODE_H_

#include <stddef.h>

#include <vector>


namespace importer {

// Given raw image data, decodes the icon, re-sampling to the correct size as
// necessary, and re-encodes as PNG data in the given output vector. Returns
// true on success.
bool ReencodeFavicon(const unsigned char* src_data,
                     size_t src_len,
                     std::vector<unsigned char>* png_data);

}  // namespace importer

#endif  // CHROME_UTILITY_IMPORTER_FAVICON_REENCODE_H_
