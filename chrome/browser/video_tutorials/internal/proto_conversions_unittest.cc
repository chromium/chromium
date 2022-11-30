// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_tutorials {
namespace {

// Verify round-way conversion of feature enum type.
TEST(VideoTutorialsProtoConversionsTest, FeatureConversion) {
  EXPECT_EQ(FeatureType::kTest,
            ToFeatureType(FromFeatureType(FeatureType::kTest)));
  EXPECT_EQ(FeatureType::kChromeIntro,
            ToFeatureType(FromFeatureType(FeatureType::kChromeIntro)));
  EXPECT_EQ(FeatureType::kDownload,
            ToFeatureType(FromFeatureType(FeatureType::kDownload)));
  EXPECT_EQ(FeatureType::kSearch,
            ToFeatureType(FromFeatureType(FeatureType::kSearch)));
  EXPECT_EQ(FeatureType::kVoiceSearch,
            ToFeatureType(FromFeatureType(FeatureType::kVoiceSearch)));
  EXPECT_EQ(FeatureType::kSummary,
            ToFeatureType(FromFeatureType(FeatureType::kSummary)));
}

TEST(VideoTutorialsProtoConversionsTest, TutorialFromProto) {
  std::string title = "some_title";
  std::string video_url = "https://some_video_url.com/";
  std::string share_url = "https://some_share_url.com/";
  std::string poster_url = "https://some_poster_url.com/";
  std::string animated_gif_url = "https://some_animated_gif_url.com/";
  std::string thumbnail_url = "https://some_thumbnail_url.com/";
  std::string caption_url = "https://caption_url.com/";
  int video_length = 10;

  proto::VideoTutorial proto;
  proto.set_feature(FromFeatureType(FeatureType::kDownload));
  proto.set_title(title);
  proto.set_video_url(video_url);
  proto.set_share_url(share_url);
  proto.set_poster_url(poster_url);
  proto.set_animated_gif_url(animated_gif_url);
  proto.set_thumbnail_url(thumbnail_url);
  proto.set_caption_url(caption_url);
  proto.set_video_length(video_length);
  Tutorial tutorial;

  TutorialFromProto(&proto, &tutorial);
  EXPECT_EQ(title, tutorial.title);
  EXPECT_EQ(video_url, tutorial.video_url);
  EXPECT_EQ(share_url, tutorial.share_url);
  EXPECT_EQ(poster_url, tutorial.poster_url);
  EXPECT_EQ(animated_gif_url, tutorial.animated_gif_url);
  EXPECT_EQ(thumbnail_url, tutorial.thumbnail_url);
  EXPECT_EQ(caption_url, tutorial.caption_url);
  EXPECT_EQ(video_length, tutorial.video_length);

  proto::VideoTutorialGroup group;
  group.add_tutorials()->set_title(title);
  std::vector<Tutorial> tutorials = TutorialsFromProto(&group);
  EXPECT_EQ(1u, tutorials.size());
  EXPECT_EQ(title, tutorials[0].title);
}

}  // namespace
}  // namespace video_tutorials
