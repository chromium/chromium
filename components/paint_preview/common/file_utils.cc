// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/file_utils.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"

namespace paint_preview {

bool WriteProtoToFile(const base::FilePath& file_path,
                      const PaintPreviewProto& proto) {
  // TODO(crbug.com/40671082): stream to file instead.
  std::string proto_str;
  if (!proto.SerializeToString(&proto_str))
    return false;
  return base::WriteFile(file_path, proto_str);
}

std::unique_ptr<PaintPreviewProto> ReadProtoFromFile(
    const base::FilePath& file_path) {
  // TODO(crbug.com/40671082): Use ZeroCopyInputStream instead.
  std::string out;
  std::unique_ptr<PaintPreviewProto> proto =
      std::make_unique<PaintPreviewProto>();
  if (!base::ReadFileToString(file_path, &out) ||
      !(proto->ParseFromString(out)))
    return nullptr;

  return proto;
}

}  // namespace paint_preview
