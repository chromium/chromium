// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/cast_media_source.h"

#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

using cast_channel::CastDeviceCapability;
using cast_channel::CastDeviceCapabilitySet;
using cast_channel::ReceiverAppType;

namespace media_router {

namespace {
void AssertDefaultCastMediaSource(CastMediaSource* source) {
  EXPECT_TRUE(source->client_id().empty());
  EXPECT_EQ(kDefaultLaunchTimeout, source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kPageScoped, source->auto_join_policy());
  EXPECT_EQ(DefaultActionPolicy::kCreateSession,
            source->default_action_policy());
  EXPECT_EQ(ReceiverAppType::kWeb, source->supported_app_types()[0]);
}
}  // namespace

TEST(CastMediaSourceTest, FromCastURLWithDefaults) {
  MediaSource::Id source_id("cast:ABCDEFAB");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(1u, source->app_infos().size());
  const CastAppInfo& app_info = source->app_infos()[0];
  EXPECT_EQ("ABCDEFAB", app_info.app_id);
  EXPECT_TRUE(app_info.required_capabilities.empty());
  EXPECT_EQ(std::nullopt, source->target_playout_delay());
  EXPECT_EQ(true, source->site_requested_audio_capture());
  EXPECT_EQ(cast_channel::VirtualConnectionType::kStrong,
            source->connection_type());
  AssertDefaultCastMediaSource(source.get());
}

TEST(CastMediaSourceTest, FromCastURL) {
  MediaSource::Id source_id(
      "cast:ABCDEFAB?capabilities=video_out,audio_out"
      "&clientId=12345"
      "&launchTimeout=30000"
      "&autoJoinPolicy=tab_and_origin_scoped"
      "&defaultActionPolicy=cast_this_tab"
      "&appParams=appParams"
      "&supportedAppTypes=ANDROID_TV,WEB"
      "&streamingTargetPlayoutDelayMillis=42"
      "&streamingCaptureAudio=0"
      "&invisibleSender=true");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(1u, source->app_infos().size());
  const CastAppInfo& app_info = source->app_infos()[0];
  EXPECT_EQ("ABCDEFAB", app_info.app_id);
  EXPECT_EQ((CastDeviceCapabilitySet{CastDeviceCapability::kVideoOut,
                                     CastDeviceCapability::kAudioOut}),
            app_info.required_capabilities);
  EXPECT_EQ("12345", source->client_id());
  EXPECT_EQ(base::Milliseconds(30000), source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kTabAndOriginScoped, source->auto_join_policy());
  EXPECT_EQ(DefaultActionPolicy::kCastThisTab, source->default_action_policy());
  EXPECT_EQ(ReceiverAppType::kAndroidTv, source->supported_app_types()[0]);
  EXPECT_EQ(ReceiverAppType::kWeb, source->supported_app_types()[1]);
  EXPECT_EQ("appParams", source->app_params());
  EXPECT_EQ(base::Milliseconds(42), source->target_playout_delay());
  EXPECT_EQ(false, source->site_requested_audio_capture());
  EXPECT_EQ(cast_channel::VirtualConnectionType::kInvisible,
            source->connection_type());
}

TEST(CastMediaSourceTest, FromLegacyCastURL) {
  MediaSource::Id source_id(
      "https://google.com/cast"
      "#__castAppId__=ABCDEFAB(video_out,audio_out)"
      "/__castAppId__=otherAppId"
      "/__castClientId__=12345"
      "/__castLaunchTimeout__=30000"
      "/__castAutoJoinPolicy__=origin_scoped"
      "/__castDefaultActionPolicy__=cast_this_tab");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(2u, source->app_infos().size());
  const CastAppInfo& app_info = source->app_infos()[0];
  EXPECT_EQ("ABCDEFAB", app_info.app_id);
  EXPECT_EQ((CastDeviceCapabilitySet{CastDeviceCapability::kVideoOut,
                                     CastDeviceCapability::kAudioOut}),
            app_info.required_capabilities);
  EXPECT_EQ("otherAppId", source->app_infos()[1].app_id);
  EXPECT_EQ("12345", source->client_id());
  EXPECT_EQ(base::Milliseconds(30000), source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kOriginScoped, source->auto_join_policy());
  EXPECT_EQ(DefaultActionPolicy::kCastThisTab, source->default_action_policy());
  EXPECT_EQ(ReceiverAppType::kWeb, source->supported_app_types()[0]);
}

TEST(CastMediaSourceTest, FromPresentationURL) {
  MediaSource::Id source_id("https://google.com");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(2u, source->app_infos().size());
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioVideoAppId(),
            source->app_infos()[0].app_id);
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioOnlyAppId(),
            source->app_infos()[1].app_id);
  AssertDefaultCastMediaSource(source.get());
}

TEST(CastMediaSourceTest, FromRemotePlaybackURL) {
  MediaSource::Id source_id(
      "remote-playback:media-session?tab_id=1&video_codec=vc&audio_codec=ac");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(2u, source->app_infos().size());
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioVideoAppId(),
            source->app_infos()[0].app_id);
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioOnlyAppId(),
            source->app_infos()[1].app_id);
}

TEST(CastMediaSourceTest, FromMirroringURN) {
  MediaSource::Id source_id("urn:x-org.chromium.media:source:tab:5");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(2u, source->app_infos().size());
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioVideoAppId(),
            source->app_infos()[0].app_id);
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioOnlyAppId(),
            source->app_infos()[1].app_id);
  AssertDefaultCastMediaSource(source.get());
}

TEST(CastMediaSourceTest, FromDesktopUrnWithoutAudio) {
  MediaSource::Id source_id("urn:x-org.chromium.media:source:desktop:foo");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(1u, source->app_infos().size());
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioVideoAppId(),
            source->app_infos()[0].app_id);
  AssertDefaultCastMediaSource(source.get());
}

TEST(CastMediaSourceTest, FromDesktopUrnWithAudio) {
  MediaSource::Id source_id(
      "urn:x-org.chromium.media:source:desktop:foo?with_audio=true");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(2u, source->app_infos().size());
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioVideoAppId(),
            source->app_infos()[0].app_id);
  EXPECT_EQ(openscreen::cast::GetCastStreamingAudioOnlyAppId(),
            source->app_infos()[1].app_id);
  AssertDefaultCastMediaSource(source.get());
}

TEST(CastMediaSourceTest, FromInvalidSource) {
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("invalid:source"));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("file:///foo.mp4"));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId(""));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("cast:"));

  // Missing app ID.
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("cast:?param=foo"));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId(
      "https://google.com/cast#__castAppId__=/param=foo"));
  // URL spec exceeds maximum size limit 64KB.
  int length = 64 * 1024 + 1;
  std::string invalidURL(length, 'a');
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("cast:appid?" + invalidURL));
}

}  // namespace media_router
