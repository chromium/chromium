// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/entity_annotator_native_library.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using EntityAnnotatorNativeLibraryTest = ::testing::Test;

TEST_F(EntityAnnotatorNativeLibraryTest, CanCreateValidLibrary) {
  std::unique_ptr<EntityAnnotatorNativeLibrary> lib =
      EntityAnnotatorNativeLibrary::Create();
  ASSERT_TRUE(lib);
  EXPECT_TRUE(lib->IsValid());
}

}  // namespace
}  // namespace optimization_guide
