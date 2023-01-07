// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/entity_annotator_native_library.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using EntityAnnotatorNativeLibraryTest = ::testing::Test;

TEST_F(EntityAnnotatorNativeLibraryTest, CanCreateValidLibrary) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EntityAnnotatorNativeLibrary> lib =
      EntityAnnotatorNativeLibrary::Create(/*should_provide_filter_path=*/true);
  ASSERT_TRUE(lib);
  EXPECT_TRUE(lib->IsValid());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.EntityAnnotatorNativeLibrary.InitiatedSuccessfully",
      true, 1);
}

}  // namespace
}  // namespace optimization_guide
