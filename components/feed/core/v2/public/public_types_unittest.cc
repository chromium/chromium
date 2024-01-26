// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

DebugStreamData MakeDebugStreamData() {
  NetworkResponseInfo fetch_info;
  fetch_info.status_code = 200;
  fetch_info.fetch_duration = base::Seconds(4);
  fetch_info.fetch_time = base::Time::UnixEpoch() + base::Minutes(200);
  fetch_info.bless_nonce = "nonce";
  fetch_info.base_request_url = GURL("https://www.google.com");

  NetworkResponseInfo upload_info;
  upload_info.status_code = 200;
  upload_info.fetch_duration = base::Seconds(2);
  upload_info.fetch_time = base::Time::UnixEpoch() + base::Minutes(201);
  upload_info.base_request_url = GURL("https://www.upload.com");

  DebugStreamData data;
  data.fetch_info = fetch_info;
  data.upload_info = upload_info;
  data.load_stream_status = "loaded OK";
  return data;
}

}  // namespace

TEST(DebugStreamData, CanSerialize) {
  const DebugStreamData test_data = MakeDebugStreamData();
  const auto serialized = SerializeDebugStreamData(test_data);
  std::optional<DebugStreamData> result =
      DeserializeDebugStreamData(serialized);
  ASSERT_TRUE(result);

  EXPECT_EQ(SerializeDebugStreamData(*result), serialized);

  ASSERT_TRUE(result->fetch_info);
  EXPECT_EQ(test_data.fetch_info->status_code, result->fetch_info->status_code);
  EXPECT_EQ(test_data.fetch_info->fetch_duration,
            result->fetch_info->fetch_duration);
  EXPECT_EQ(test_data.fetch_info->fetch_time, result->fetch_info->fetch_time);
  EXPECT_EQ(test_data.fetch_info->bless_nonce, result->fetch_info->bless_nonce);
  EXPECT_EQ(test_data.fetch_info->base_request_url,
            result->fetch_info->base_request_url);

  ASSERT_TRUE(result->upload_info);
  EXPECT_EQ(test_data.upload_info->status_code,
            result->upload_info->status_code);
  EXPECT_EQ(test_data.upload_info->fetch_duration,
            result->upload_info->fetch_duration);
  EXPECT_EQ(test_data.upload_info->fetch_time, result->upload_info->fetch_time);
  EXPECT_EQ(test_data.upload_info->bless_nonce,
            result->upload_info->bless_nonce);
  EXPECT_EQ(test_data.upload_info->base_request_url,
            result->upload_info->base_request_url);

  EXPECT_EQ(test_data.load_stream_status, result->load_stream_status);
}

TEST(DebugStreamData, CanSerializeWithoutFetchInfo) {
  DebugStreamData input = MakeDebugStreamData();
  input.fetch_info = std::nullopt;

  const auto serialized = SerializeDebugStreamData(input);
  std::optional<DebugStreamData> result =
      DeserializeDebugStreamData(serialized);
  ASSERT_TRUE(result);

  EXPECT_EQ(SerializeDebugStreamData(*result), serialized);
}

TEST(DebugStreamData, CanSerializeWithoutUploadInfo) {
  DebugStreamData input = MakeDebugStreamData();
  input.upload_info = std::nullopt;

  const auto serialized = SerializeDebugStreamData(input);
  std::optional<DebugStreamData> result =
      DeserializeDebugStreamData(serialized);
  ASSERT_TRUE(result);

  EXPECT_EQ(SerializeDebugStreamData(*result), serialized);
}

TEST(DebugStreamData, FailsDeserializationGracefully) {
  ASSERT_EQ(std::nullopt, DeserializeDebugStreamData({}));
}

TEST(WebFeedPageInformation, SetUrlStripsFragment) {
  WebFeedPageInformation info;
  info.SetUrl(GURL("https://chromium.org#1"));
  EXPECT_EQ(GURL("https://chromium.org"), info.url());
}

TEST(WebFeedPageInformation, SetCanonicalUrlStripsFragment) {
  WebFeedPageInformation info;
  info.SetCanonicalUrl(GURL("https://chromium.org#1"));
  EXPECT_EQ(GURL("https://chromium.org"), info.canonical_url());
}

}  // namespace feed
