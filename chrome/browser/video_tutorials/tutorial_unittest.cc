// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/tutorial.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace video_tutorials {
namespace {

const char kTestTitle[] = "Test Title";
const char kTestURL[] = "https://www.example.com";

void ResetTutorialEntry(Tutorial* entry) {
  *entry = Tutorial(FeatureType::kTest, kTestTitle, kTestURL, kTestURL,
                    kTestURL, kTestURL, kTestURL, kTestURL, 60);
}

// Verify the copy/assign and compare operators for Tutorial struct.
TEST(VideoTutorialsTest, CopyAndCompareOperators) {
  Tutorial lhs, rhs;
  ResetTutorialEntry(&lhs);
  ResetTutorialEntry(&rhs);
  EXPECT_EQ(lhs, rhs);

  rhs.title = "changed";
  EXPECT_NE(lhs, rhs);
  ResetTutorialEntry(&rhs);

  rhs.feature = FeatureType::kDownload;
  EXPECT_NE(lhs, rhs);
  ResetTutorialEntry(&rhs);

  rhs.video_url = GURL("changed");
  EXPECT_NE(lhs, rhs);
  ResetTutorialEntry(&rhs);

  rhs.share_url = GURL("changed");
  EXPECT_NE(lhs, rhs);
  ResetTutorialEntry(&rhs);

  rhs.poster_url = GURL("changed");
  EXPECT_NE(lhs, rhs);
  ResetTutorialEntry(&rhs);

  rhs.caption_url = GURL("changed");
  EXPECT_NE(lhs, rhs);
  ResetTutorialEntry(&rhs);

  rhs.video_length++;
  EXPECT_NE(lhs, rhs);
}

}  // namespace
}  // namespace video_tutorials
