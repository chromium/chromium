// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_FILE_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_FILE_UTILS_H_

#include <memory>

#include "base/files/file_path.h"

namespace paint_preview {

class PaintPreviewProto;

// Writes |proto| to |file_path|. Returns false on failure.
bool WriteProtoToFile(const base::FilePath& file_path,
                      const PaintPreviewProto& proto);

// Reads a PaintPreviewProto from |file_path|. Returns nullptr in case of
// failure.
std::unique_ptr<PaintPreviewProto> ReadProtoFromFile(
    const base::FilePath& file_path);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_FILE_UTILS_H_
