// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UTIL_IMAGE_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_UTIL_IMAGE_UTIL_H_

#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace webui {
std::string MakeDataURIForImage(base::span<const uint8_t> image_data,
                                base::StringPiece mime_subtype);

std::string EncodePNGAndMakeDataURI(gfx::ImageSkia image, float scale_factor);
}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_UTIL_IMAGE_UTIL_H_
