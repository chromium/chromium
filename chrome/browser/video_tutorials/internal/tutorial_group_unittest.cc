// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_group.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace video_tutorials {
namespace {

void ResetTutorialGroup(TutorialGroup* group) {
  *group = TutorialGroup("en");
  group->tutorials.resize(3, Tutorial());
  group->tutorials.front().feature = FeatureType::kDownload;
  group->tutorials.back().feature = FeatureType::kSearch;
}

// Verify the copy/assign and compare operators for TutorialGroup struct.
TEST(VideoTutorialGroupTest, CopyAndCompareOperators) {
  TutorialGroup lhs, rhs;
  ResetTutorialGroup(&lhs);
  ResetTutorialGroup(&rhs);

  EXPECT_EQ(lhs, rhs);

  rhs.language = "jp";
  EXPECT_NE(lhs, rhs);
  ResetTutorialGroup(&rhs);

  std::reverse(rhs.tutorials.begin(), rhs.tutorials.end());
  EXPECT_NE(lhs, rhs);
  ResetTutorialGroup(&rhs);

  rhs.tutorials.pop_back();
  EXPECT_NE(lhs, rhs);
  ResetTutorialGroup(&rhs);
}

}  // namespace
}  // namespace video_tutorials
