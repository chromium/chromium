// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/discovery/media_sink_internal.h"

#include "components/media_router/common/test/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kSinkId[] = "sinkId123";
constexpr char kSinkName[] = "The sink";
constexpr char kIPAddress[] = "192.168.1.2";
constexpr char kModelName[] = "model name";
constexpr char kAppUrl[] = "https://example.com";

media_router::DialSinkExtraData CreateDialSinkExtraData(
    const std::string& model_name,
    const std::string& ip_address,
    const std::string& app_url) {
  media_router::DialSinkExtraData dial_extra_data;
  EXPECT_TRUE(dial_extra_data.ip_address.AssignFromIPLiteral(ip_address));
  dial_extra_data.model_name = model_name;
  dial_extra_data.app_url = GURL(app_url);

  return dial_extra_data;
}

media_router::CastSinkExtraData CreateCastSinkExtraData(
    const std::string& model_name,
    const std::string& ip_address,
    uint8_t capabilities,
    int cast_channel_id) {
  media_router::CastSinkExtraData cast_extra_data;
  net::IPAddress ip;
  EXPECT_TRUE(ip.AssignFromIPLiteral(ip_address));
  cast_extra_data.ip_endpoint = net::IPEndPoint(ip, 1234);
  cast_extra_data.model_name = model_name;
  cast_extra_data.capabilities = {
      cast_channel::CastDeviceCapability::kVideoOut};
  cast_extra_data.cast_channel_id = 3;
  return cast_extra_data;
}

// static
media_router::DialSinkExtraData CreateDialSinkExtraData() {
  return CreateDialSinkExtraData(kModelName, kIPAddress, kAppUrl);
}

// static
media_router::CastSinkExtraData CreateCastSinkExtraData() {
  return CreateCastSinkExtraData(kModelName, kIPAddress, 2, 3);
}

}  // namespace

namespace media_router {

TEST(MediaSinkInternalTest, TestIsValidSinkId) {
  EXPECT_FALSE(MediaSinkInternal::IsValidSinkId(""));
  EXPECT_TRUE(MediaSinkInternal::IsValidSinkId("rjuKv_yxhY4jg7QBIp0kbngLjR6A"));
}

TEST(MediaSinkInternalTest, TestConstructorAndAssignment) {
  MediaSink sink{CreateCastSink(kSinkId, kSinkName)};
  DialSinkExtraData dial_extra_data = CreateDialSinkExtraData();
  CastSinkExtraData cast_extra_data = CreateCastSinkExtraData();

  MediaSinkInternal generic_sink;
  generic_sink.set_sink(sink);
  MediaSinkInternal dial_sink(sink, dial_extra_data);
  MediaSinkInternal cast_sink(sink, cast_extra_data);

  MediaSinkInternal copied_generic_sink(generic_sink);
  MediaSinkInternal copied_dial_sink(dial_sink);
  MediaSinkInternal copied_cast_sink(cast_sink);

  ASSERT_TRUE(generic_sink == copied_generic_sink);
  ASSERT_TRUE(dial_sink == copied_dial_sink);
  ASSERT_TRUE(cast_sink == copied_cast_sink);

  MediaSinkInternal assigned_empty_sink;
  MediaSinkInternal assigned_generic_sink = generic_sink;
  MediaSinkInternal assigned_dial_sink = dial_sink;
  MediaSinkInternal assigned_cast_sink = cast_sink;

  std::vector<MediaSinkInternal> assigned_sinks(
      {assigned_empty_sink, assigned_generic_sink, assigned_dial_sink,
       assigned_cast_sink});
  std::vector<MediaSinkInternal> original_sinks(
      {generic_sink, dial_sink, cast_sink});

  for (auto& actual_sink : assigned_sinks) {
    for (const auto& original_sink : original_sinks) {
      actual_sink = original_sink;
      EXPECT_EQ(original_sink, actual_sink);
    }
  }
}

TEST(MediaSinkInternalTest, TestSetExtraData) {
  MediaSink sink{CreateCastSink(kSinkId, kSinkName)};
  DialSinkExtraData dial_extra_data = CreateDialSinkExtraData();
  CastSinkExtraData cast_extra_data = CreateCastSinkExtraData();

  MediaSinkInternal dial_sink1;
  dial_sink1.set_dial_data(dial_extra_data);
  ASSERT_EQ(dial_extra_data, dial_sink1.dial_data());

  MediaSinkInternal cast_sink1;
  cast_sink1.set_cast_data(cast_extra_data);
  ASSERT_EQ(cast_extra_data, cast_sink1.cast_data());

  DialSinkExtraData dial_extra_data2 = CreateDialSinkExtraData(
      "new_dial_model_name", "192.1.2.100", "https://example2.com");
  CastSinkExtraData cast_extra_data2 =
      CreateCastSinkExtraData("new_cast_model_name", "192.1.2.101", 4, 5);

  MediaSinkInternal dial_sink2(sink, dial_extra_data);
  dial_sink2.set_dial_data(dial_extra_data2);
  ASSERT_EQ(dial_extra_data2, dial_sink2.dial_data());

  MediaSinkInternal cast_sink2(sink, cast_extra_data);
  cast_sink2.set_cast_data(cast_extra_data2);
  ASSERT_EQ(cast_extra_data2, cast_sink2.cast_data());
}

TEST(MediaSinkInternalTest, TestProcessDeviceUUID) {
  EXPECT_EQ("de51d94921f15f8af6dbf65592bb3610",
            MediaSinkInternal::ProcessDeviceUUID(
                "uuid:de51d949-21f1-5f8a-f6db-f65592bb3610"));
  EXPECT_EQ("de51d94921f15f8af6dbf65592bb3610",
            MediaSinkInternal::ProcessDeviceUUID(
                "de51d94921f1-5f8a-f6db-f65592bb3610"));
  EXPECT_EQ(
      "de51d94921f15f8af6dbf65592bb3610",
      MediaSinkInternal::ProcessDeviceUUID("DE51D94921F15F8AF6DBF65592BB3610"));
  EXPECT_EQ("abc:de51d94921f15f8af6dbf65592bb3610",
            MediaSinkInternal::ProcessDeviceUUID(
                "abc:de51d949-21f1-5f8a-f6db-f65592bb3610"));
  EXPECT_EQ("", MediaSinkInternal::ProcessDeviceUUID(""));
}

}  // namespace media_router
