// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_source.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
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
  EXPECT_TRUE(IsValidPresentationUrl(GURL("remote-playback:foo")));
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

TEST(MediaSourceTest, ForAnyTab) {
  auto source = MediaSource::ForAnyTab();
  EXPECT_EQ("urn:x-org.chromium.media:source:tab:*", source.id());
  EXPECT_FALSE(source.TabId().has_value());
  EXPECT_FALSE(source.IsDesktopMirroringSource());
  EXPECT_TRUE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
  EXPECT_FALSE(source.IsRemotePlaybackSource());
}

TEST(MediaSourceTest, ForTab) {
  auto source = MediaSource::ForTab(123);
  EXPECT_EQ("urn:x-org.chromium.media:source:tab:123", source.id());
  EXPECT_EQ(123, source.TabId().value_or(-1));
  EXPECT_FALSE(source.IsDesktopMirroringSource());
  EXPECT_TRUE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
  EXPECT_FALSE(source.IsRemotePlaybackSource());
}

TEST(MediaSourceTest, TabMirroringSourceTabId) {
  MediaSource source = MediaSource("");
  EXPECT_FALSE(source.TabId().has_value());
  EXPECT_FALSE(source.IsTabMirroringSource());

  source = MediaSource("urn:x-org.chromium.media:source:invalid:123");
  EXPECT_FALSE(source.TabId().has_value());
  EXPECT_FALSE(source.IsTabMirroringSource());

  source = MediaSource("urn:x-org.chromium.media:source:tab:abc");
  EXPECT_FALSE(source.TabId().has_value());
  EXPECT_FALSE(source.IsTabMirroringSource());

  source = MediaSource("urn:x-org.chromium.media:source:tab:123");
  EXPECT_EQ(123, source.TabId().value_or(-1));
  EXPECT_TRUE(source.IsTabMirroringSource());
}

TEST(MediaSourceTest, RemotePlaybackSourceTabId) {
  MediaSource source = MediaSource("");
  EXPECT_FALSE(source.TabIdFromRemotePlaybackSource().has_value());
  EXPECT_FALSE(source.IsRemotePlaybackSource());

  source = MediaSource(
      "remote-playback:media-session?&video_codec=vp8&audio_codec=aac");
  EXPECT_FALSE(source.TabIdFromRemotePlaybackSource().has_value());
  EXPECT_TRUE(source.IsRemotePlaybackSource());

  source = MediaSource(
      "remote-playback:media-session?tab_id=abc&video_codec=vp8&audio_codec="
      "aac");
  EXPECT_FALSE(source.TabIdFromRemotePlaybackSource().has_value());
  EXPECT_TRUE(source.IsRemotePlaybackSource());

  source = MediaSource(
      "remote-playback:media-session?tab_id=123&video_codec=vp8&audio_codec="
      "aac");
  EXPECT_EQ(123, source.TabIdFromRemotePlaybackSource().value_or(-1));
  EXPECT_TRUE(source.IsRemotePlaybackSource());
}

TEST(MediaSourceTest, ForDesktopWithoutAudio) {
  std::string media_id = "fakeMediaId";
  auto source = MediaSource::ForDesktop(media_id, false);
  EXPECT_EQ("urn:x-org.chromium.media:source:desktop:" + media_id, source.id());
  EXPECT_TRUE(source.IsDesktopMirroringSource());
  EXPECT_EQ(media_id, source.DesktopStreamId());
  EXPECT_FALSE(source.IsDesktopSourceWithAudio());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
  EXPECT_FALSE(source.IsRemotePlaybackSource());
}

TEST(MediaSourceTest, ForDesktopWithAudio) {
  std::string media_id = "fakeMediaId";
  auto source = MediaSource::ForDesktop(media_id, true);
  EXPECT_EQ("urn:x-org.chromium.media:source:desktop:" + media_id +
                "?with_audio=true",
            source.id());
  EXPECT_TRUE(source.IsDesktopMirroringSource());
  EXPECT_EQ(media_id, source.DesktopStreamId());
  EXPECT_TRUE(source.IsDesktopSourceWithAudio());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
  EXPECT_FALSE(source.IsRemotePlaybackSource());
}

TEST(MediaSourceTest, ForUnchosenDesktop) {
  base::test::ScopedFeatureList scoped_features;
  auto enabled_features = std::vector<base::test::FeatureRef>(
      {media::kCastLoopbackAudioToAudioReceivers});
#if BUILDFLAG(IS_LINUX)
  enabled_features.push_back(media::kPulseaudioLoopbackForCast);
#endif
  scoped_features.InitWithFeatures(enabled_features, {});

  auto source = MediaSource::ForUnchosenDesktop();
  EXPECT_TRUE(source.IsDesktopMirroringSource());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
  EXPECT_FALSE(source.IsRemotePlaybackSource());

  if (media::IsSystemLoopbackCaptureSupported()) {
    EXPECT_TRUE(source.IsDesktopSourceWithAudio());
  } else {
    EXPECT_FALSE(source.IsDesktopSourceWithAudio());
  }
}

TEST(MediaSourceTest, ForPresentationUrl) {
  constexpr char kPresentationUrl[] =
      "https://www.example.com/presentation.html";
  auto source = MediaSource::ForPresentationUrl(GURL(kPresentationUrl));
  EXPECT_EQ(kPresentationUrl, source.id());
  EXPECT_FALSE(source.IsDesktopMirroringSource());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
  EXPECT_FALSE(source.IsRemotePlaybackSource());
}

TEST(MediaSourceTest, ForRemotePlayback) {
  constexpr char kRemotePlaybackUrl[] =
      "remote-playback:media-session?video_codec=vp8&audio_codec=aac&tab_id=1";
  auto source = MediaSource::ForRemotePlayback(1, media::VideoCodec::kVP8,
                                               media::AudioCodec::kAAC);
  EXPECT_EQ(kRemotePlaybackUrl, source.id());
  EXPECT_EQ(1, source.TabIdFromRemotePlaybackSource());
  EXPECT_FALSE(source.IsDesktopMirroringSource());
  EXPECT_FALSE(source.IsTabMirroringSource());
  EXPECT_FALSE(source.IsCastPresentationUrl());
  EXPECT_FALSE(source.IsDialSource());
  EXPECT_TRUE(source.IsRemotePlaybackSource());
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
