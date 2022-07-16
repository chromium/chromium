// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UTILS_PDF_CONVERSION_H_
#define CHROMEOS_UTILS_PDF_CONVERSION_H_

#include <string>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {

// Converts `jpg_images` to a single PDF, and writes the PDF to `file_path`. If
// `rotate_alternate_pages` is true, every other page is rotated 180 degrees. If
// the DPI is specified, use it to calculate proper page and media box size.
// Returns whether the PDF was successfully saved.
bool ConvertJpgImagesToPdf(const std::vector<std::string>& jpg_images,
                           const base::FilePath& file_path,
                           bool rotate_alternate_pages,
                           absl::optional<int> dpi);

// Converts `jpg_image` to a single PDF, and saved the result into `output`.
bool ConvertJpgImageToPdf(const std::vector<uint8_t>& jpg_image,
                          std::vector<uint8_t>* output);

}  // namespace chromeos

#endif  // CHROMEOS_UTILS_PDF_CONVERSION_H_
