// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_tutorials {
namespace {

// Verify round-way conversion of Tutorial struct.
TEST(VideoTutorialsProtoConversionsTest, TutorialConversion) {
  Tutorial expected, actual;
  test::BuildTestEntry(&expected);
  TutorialProto intermediate;
  TutorialToProto(&expected, &intermediate);
  TutorialFromProto(&intermediate, &actual);
  EXPECT_EQ(expected, actual);
}

// Verify round-way conversion of TutorialGroup struct.
TEST(VideoTutorialsProtoConversionsTest, TutorialGroupConversion) {
  TutorialGroup expected, actual;
  test::BuildTestGroup(&expected);
  TutorialGroupProto intermediate;
  TutorialGroupToProto(&expected, &intermediate);
  TutorialGroupFromProto(&intermediate, &actual);
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace video_tutorials
