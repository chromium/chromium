// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/media_source.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaSourceTest, IsLegacyCastPresentationUrl) {
  EXPECT_TRUE(IsLegacyCastPresentationUrl(
      GURL("https://google.com/cast#__castAppId__=theAppId")));
  EXPECT_TRUE(IsLegacyCastPresentationUrl(
      GURL("HTTPS://GOOGLE.COM/CAST#__CASTAPPID__=theAppId")));
  EXPECT_FALSE(IsLegacyCastPresentationUrl(
      GURL("https://google.com/cast#__castAppId__")));
}

TEST(MediaSourceTest, IsValidPresentationUrl) {
  EXPECT_FALSE(IsValidPresentationUrl(GURL()));
  EXPECT_FALSE(IsValidPresentationUrl(GURL("unsupported-scheme://foo")));

  EXPECT_TRUE(IsValidPresentationUrl(GURL("https://google.com")));
  EXPECT_TRUE(IsValidPresentationUrl(GURL("cast://foo")));
  EXPECT_TRUE(IsValidPresentationUrl(GURL("cast:foo")));
}

TEST(MediaSourceTest, IsAutoJoinPresentationId) {
  EXPECT_TRUE(IsAutoJoinPresentationId("auto-join"));
  EXPECT_FALSE(IsAutoJoinPresentationId("not-auto-join"));
}

TEST(MediaSourceTest, Constructor) {
  // Test that the object's getters match the constructor parameters.
  MediaSource source1("urn:x-com.google.cast:application:DEADBEEF");
  EXPECT_EQ("urn:x-com.google.cast:application:DEADBEEF", source1.id());
  EXPECT_EQ(GURL(""), source1.url());
}

TEST(MediaSourceTest, ConstructorWithGURL) {
  GURL test_url = GURL("http://google.com");
  MediaSource source1(test_url);
  EXPECT_EQ(test_url.spec(), source1.id());
  EXPECT_EQ(test_url, source1.url());
}

TEST(MediaSourceTest, ConstructorWithURLString) {
  GURL test_url = GURL("http://google.com");
  MediaSource source1(test_url.spec());
  EXPECT_EQ(test_url.spec(), source1.id());
  EXPECT_EQ(test_url, source1.url());
}

TEST(MediaSourceTest, ForTab) {
  auto source = MediaSource::ForTab(123);
  EXPECT_EQ("urn:x-org.chromium.media:source:tab:123", source.id());
  EXPECT_EQ(123, source.TabId());
  EXPECT_TRUE(source.IsValid());
  EXPECT_FALSE(source.IsDesktopMirroringSource());
  EXPECT_TRUE(source.IsTabMirroringSource());
  EXPECT_TRUE(source.IsMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
}

TEST(MediaSourceTest, ForTabContentRemoting) {
  auto source = MediaSource::ForTabContentRemoting(123);
  EXPECT_EQ(123, source.TabId());
  EXPECT_TRUE(source.IsValid());
  EXPECT_FALSE(source.IsDesktopMirroringSource());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
}

TEST(MediaSourceTest, ForDesktop) {
  std::string media_id = "fakeMediaId";
  auto source = MediaSource::ForDesktop(media_id);
  EXPECT_EQ("urn:x-org.chromium.media:source:desktop:" + media_id, source.id());
  EXPECT_TRUE(source.IsValid());
  EXPECT_TRUE(source.IsDesktopMirroringSource());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_TRUE(source.IsMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
}

TEST(MediaSourceTest, ForPresentationUrl) {
  constexpr char kPresentationUrl[] =
      "https://www.example.com/presentation.html";
  auto source = MediaSource::ForPresentationUrl(GURL(kPresentationUrl));
  EXPECT_EQ(kPresentationUrl, source.id());
  EXPECT_TRUE(source.IsValid());
  EXPECT_FALSE(source.IsDesktopMirroringSource());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
}

TEST(MediaSourceTest, IsValid) {
  // Disallowed scheme
  EXPECT_FALSE(MediaSource::ForPresentationUrl(GURL("file:///some/local/path"))
                   .IsValid());
  // Not a URL
  EXPECT_FALSE(
      MediaSource::ForPresentationUrl(GURL("totally not a url")).IsValid());
}

TEST(MediaSourceTest, IsCastPresentationUrl) {
  EXPECT_TRUE(MediaSource(GURL("cast:233637DE")).IsCastPresentationUrl());
  EXPECT_TRUE(
      MediaSource(GURL("https://google.com/cast#__castAppId__=233637DE"))
          .IsCastPresentationUrl());
  // false scheme
  EXPECT_FALSE(
      MediaSource(GURL("http://google.com/cast#__castAppId__=233637DE"))
          .IsCastPresentationUrl());
  // false domain
  EXPECT_FALSE(
      MediaSource(GURL("https://google2.com/cast#__castAppId__=233637DE"))
          .IsCastPresentationUrl());
  // empty path
  EXPECT_FALSE(
      MediaSource(GURL("https://www.google.com")).IsCastPresentationUrl());
  // false path
  EXPECT_FALSE(
      MediaSource(GURL("https://www.google.com/path")).IsCastPresentationUrl());

  EXPECT_FALSE(MediaSource(GURL("")).IsCastPresentationUrl());
}

TEST(MediaSourceTest, IsDialSource) {
  EXPECT_TRUE(
      MediaSource("cast-dial:YouTube?dialPostData=postData&clientId=1234")
          .IsDialSource());
  // false scheme
  EXPECT_FALSE(MediaSource("https://google.com/cast#__castAppId__=233637DE")
                   .IsDialSource());
}

TEST(MediaSourceTest, AppNameFromDialSource) {
  MediaSource media_source(
      "cast-dial:YouTube?dialPostData=postData&clientId=1234");
  EXPECT_EQ("YouTube", media_source.AppNameFromDialSource());

  media_source = MediaSource("dial:YouTube");
  EXPECT_TRUE(media_source.AppNameFromDialSource().empty());

  media_source = MediaSource("https://google.com/cast#__castAppId__=233637DE");
  EXPECT_TRUE(media_source.AppNameFromDialSource().empty());
}

}  // namespace media_router
