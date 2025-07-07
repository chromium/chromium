// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAVICON_REENCODE_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAVICON_REENCODE_H_

#include <stddef.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"

namespace importer {

// Given raw image data, decodes the icon, re-sampling to the correct size as
// necessary, and re-encodes as PNG data. Returns the PNG data if successful,
// nullopt if not.
std::optional<std::vector<uint8_t>> ReencodeFavicon(
    base::span<const uint8_t> src);

}  // namespace importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAVICON_REENCODE_H_
