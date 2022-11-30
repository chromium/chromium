// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/test_utils.h"

std::string PersistenceParamToString(
    const ::testing::TestParamInfo<paint_preview::RecordingPersistence>&
        persistence) {
  switch (persistence.param) {
    case paint_preview::RecordingPersistence::kFileSystem:
      return "FileSystem";
    case paint_preview::RecordingPersistence::kMemoryBuffer:
      return "MemoryBuffer";
  }
}
