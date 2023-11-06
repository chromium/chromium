// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODING_TYPES_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODING_TYPES_H_

#include <cstdint>
#include <vector>

#include "chromeos/ash/services/recording/rgb_video_frame.h"

namespace recording {

// The GIF specs specify a maximum of 12 bits per LZW compression code, so a
// 16-bit unsigned integer is perfect for this type.
using LzwCode = uint16_t;

// We have a maximum of 256 colors in our color palette, so an 8-bit unsigned
// integer is enough to represent indices to these colors.
using ColorIndex = uint8_t;
using ColorIndices = std::vector<ColorIndex>;

// Defines a type for a color palette table which will eventually be written to
// the GIF file.
using ColorTable = std::vector<RgbColor>;

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODING_TYPES_H_
