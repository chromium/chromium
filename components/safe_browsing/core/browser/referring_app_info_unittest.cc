// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/referring_app_info.h"

#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

TEST(ReferringAppInfoTest, EmptyInfo) {
  internal::ReferringAppInfo info;
  EXPECT_FALSE(info.has_referring_app());
  EXPECT_FALSE(info.has_referring_webapk());
  EXPECT_EQ(
      internal::ReferringAppInfoToResult(info),
      internal::GetReferringAppInfoResult::kReferringAppNotFoundWebapkNotFound);
}

TEST(ReferringAppInfoTest, ReferringAppOnly) {
  internal::ReferringAppInfo info;
  info.referring_app_name = "test.app.example";
  info.referring_app_source = safe_browsing::ReferringAppInfo::KNOWN_APP_ID;
  EXPECT_TRUE(info.has_referring_app());
  EXPECT_FALSE(info.has_referring_webapk());
  EXPECT_EQ(
      internal::ReferringAppInfoToResult(info),
      internal::GetReferringAppInfoResult::kReferringAppFoundWebapkNotFound);
}

TEST(ReferringAppInfoTest, ReferringWebapkOnly) {
  internal::ReferringAppInfo info;
  info.referring_webapk_start_url = GURL("https://app.example.test");
  EXPECT_FALSE(info.has_referring_app());
  EXPECT_TRUE(info.has_referring_webapk());
  EXPECT_EQ(
      internal::ReferringAppInfoToResult(info),
      internal::GetReferringAppInfoResult::kReferringAppNotFoundWebapkFound);
}

TEST(ReferringAppInfoTest, ReferringAppAndWebapk) {
  internal::ReferringAppInfo info;
  info.referring_app_name = "test.app.example";
  info.referring_webapk_start_url = GURL("https://app.example.test");
  EXPECT_TRUE(info.has_referring_app());
  EXPECT_TRUE(info.has_referring_webapk());
  EXPECT_EQ(internal::ReferringAppInfoToResult(info),
            internal::GetReferringAppInfoResult::kReferringAppFoundWebapkFound);
}

TEST(ReferringAppInfoTest, CreateProto) {
  internal::ReferringAppInfo info;
  info.referring_app_name = "test.app.example";
  info.referring_app_source = safe_browsing::ReferringAppInfo::KNOWN_APP_ID;
  info.referring_webapk_start_url = GURL("https://app.example.test");
  info.referring_webapk_manifest_id = GURL("https://app.example.test/id");

  ReferringAppInfo referring_app_info_proto = GetReferringAppInfoProto(info);
  EXPECT_TRUE(referring_app_info_proto.has_referring_app_name());
  EXPECT_EQ(referring_app_info_proto.referring_app_name(), "test.app.example");
  EXPECT_EQ(referring_app_info_proto.referring_app_source(),
            safe_browsing::ReferringAppInfo::KNOWN_APP_ID);
  EXPECT_TRUE(referring_app_info_proto.has_referring_webapk());
  EXPECT_EQ(referring_app_info_proto.referring_webapk().start_url_origin(),
            "app.example.test");
  EXPECT_EQ(referring_app_info_proto.referring_webapk().id_or_start_path(),
            "id");
}

}  // namespace
}  // namespace safe_browsing
