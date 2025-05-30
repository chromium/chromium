// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_

#include <string>

#include "base/test/protobuf_matchers.h"
#include "components/paint_preview/common/serialized_recording.h"

namespace paint_preview {

using base::test::EqualsProto;

// Allow |Persistence| to be stringified in gtest.
std::string PersistenceParamToString(
    const ::testing::TestParamInfo<paint_preview::RecordingPersistence>&
        persistence);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_
