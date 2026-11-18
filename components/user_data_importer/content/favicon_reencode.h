// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAVICON_REENCODE_H_
#define COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAVICON_REENCODE_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"

namespace importer {

// Given raw image data, decodes the icon using Blink's image decoder,
// re-sampling to the correct size as necessary, and re-encodes as PNG data.
// Returns the PNG data if successful, nullopt if not.
// Because this decodes untrusted image bytes via Blink, it must only be called
// in a sandboxed process.
std::optional<std::vector<uint8_t>> ReencodeFavicon(
    base::span<const uint8_t> src);

// Batch helper that calls `ReencodeFavicon` on each entry in `raw_favicons`,
// returning a map of successfully re-encoded PNGs keyed by the same IDs.
// Must only be called in a sandboxed process.
base::flat_map<uint32_t, std::vector<uint8_t>> ReencodeFavicons(
    const base::flat_map<uint32_t, std::vector<uint8_t>>& raw_favicons);

}  // namespace importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_CONTENT_FAVICON_REENCODE_H_
