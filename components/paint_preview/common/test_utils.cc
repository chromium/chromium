// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/test_utils.h"

#include <string_view>

namespace paint_preview {

std::string_view PersistenceToString(RecordingPersistence persistence) {
  switch (persistence) {
    case RecordingPersistence::kFileSystem:
      return "FileSystem";
    case RecordingPersistence::kMemoryBuffer:
      return "MemoryBuffer";
  }
}

}  // namespace paint_preview
