// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_tutorials {
namespace {

// Verify round-way conversion of feature enum type.
TEST(VideoTutorialsProtoConversionsTest, FeatureConversion) {
  Tutorial expected, actual;
  TutorialProto intermediate;
  FeatureType features[] = {FeatureType::kTest, FeatureType::kInvalid,
                            FeatureType::kDebug, FeatureType::kDownload,
                            FeatureType::kSearch};
  for (FeatureType feature : features) {
    expected.feature = feature;
    TutorialToProto(&expected, &intermediate);
    TutorialFromProto(&intermediate, &actual);
    EXPECT_EQ(expected, actual);
  }
}

// Verify round-way conversion of Language struct.
TEST(VideoTutorialsProtoConversionsTest, LanguageConversion) {
  Language expected, actual;
  expected.locale = "jp";
  expected.name = "Japanese";
  expected.native_name = "Japanese-Native";
  LanguageProto intermediate;
  LanguageToProto(&expected, &intermediate);
  LanguageFromProto(&intermediate, &actual);
  EXPECT_EQ(expected, actual);
}

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

// Verify server response to client conversion.
TEST(VideoTutorialsProtoConversionsTest, ServerResponseToClientConversion) {
  ServerResponseProto server_response;
  std::vector<TutorialGroup> groups;
  server_response.add_tutorial_groups();
  server_response.add_tutorial_groups();
  TutorialGroupsFromServerResponseProto(&server_response, &groups);
  EXPECT_EQ(groups.size(), 2u);
}

}  // namespace
}  // namespace video_tutorials
