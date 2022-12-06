// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/asset_link_data.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::IsEmpty;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(AssetLinkData, NonJSON) {
  constexpr char json[] = R"([trash])";
  AssetLinkData data;
  EXPECT_FALSE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, NotList) {
  constexpr char json[] =
      R"({
      "include": "https://example/.well-known/assetlinks.json"
  })";
  AssetLinkData data;
  EXPECT_FALSE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, IncludeWrongValue) {
  constexpr char json[] =
      R"([{
      "include": 24
  }])";
  AssetLinkData data;
  EXPECT_FALSE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, IncludeFile) {
  constexpr char json[] =
      R"([{
      "include": "https://example/.well-known/assetlinks.json"
  }])";
  AssetLinkData data;
  EXPECT_TRUE(data.Parse(json));
  EXPECT_THAT(data.includes(),
              ElementsAre(GURL("https://example/.well-known/assetlinks.json")));
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, IncludeHTTPFile) {
  constexpr char json[] =
      R"([{
      "include": "http://example/.well-known/assetlinks.json"
  }])";
  AssetLinkData data;
  EXPECT_TRUE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, IncludeInvalidFile) {
  constexpr char json[] =
      R"([{
      "include": "www.example/assetlinks.json"
  }])";
  AssetLinkData data;
  data.Parse(json);
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, HandleURLsPermission) {
  constexpr char json[] =
      R"([{
  "relation": ["delegate_permission/common.handle_all_urls"],
  "target": {
    "namespace": "web",
    "site": "https://www.google.com"
  }
  },{
  "relation": ["delegate_permission/common.handle_all_urls"],
  "target": {
    "namespace": "android_app",
    "package_name": "org.digitalassetlinks.sampleapp",
    "sha256_cert_fingerprints": ["10:39:38:EE:45:37:E5:9E:8E:E7:92:F6"]
  }
  }])";
  AssetLinkData data;
  EXPECT_TRUE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, BrokenRelation) {
  constexpr char json[] =
      R"([{
  "relation": "delegate_permission/common.get_login_creds",
  "target": {
    "namespace": "web",
    "site": "https://www.google.com"
  }
  }])";
  AssetLinkData data;
  EXPECT_FALSE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), IsEmpty());
}

TEST(AssetLinkData, GetLoginCredsPermission) {
  constexpr char json[] =
      R"([{
  "relation": ["delegate_permission/common.get_login_creds"],
  "target": {
    "namespace": "web",
    "site": "https://www.google.com"
  }
  },{
  "relation": ["delegate_permission/common.get_login_creds"],
  "target": {
    "namespace": "android_app",
    "package_name": "org.digitalassetlinks.sampleapp",
    "sha256_cert_fingerprints": ["10:39:38:EE:45:37:E5:9E:8E:E7:92:F6"]
  }
  },{
  "relation": ["delegate_permission/common.get_login_creds"],
  "target": {
    "namespace": "web",
    "site": "https://www.google.ru"
  }
  },{
  "relation": ["delegate_permission/common.get_login_creds"],
  "target": {
    "namespace": "web",
    "site": "http://example.com"
  }
  }
  ])";
  AssetLinkData data;
  EXPECT_TRUE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(),
              UnorderedElementsAre(GURL("https://www.google.com"),
                                   GURL("https://www.google.ru")));
}

TEST(AssetLinkData, MultiplePermissions) {
  constexpr char json[] =
      R"([{
  "relation": ["something","delegate_permission/common.get_login_creds"],
  "target": {
    "namespace": "web",
    "site": "https://www.google.com"
  }
  },{
  "relation": ["delegate_permission/common.get_login_creds",
               "delegate_permission/common.handle_all_urls"],
  "target": {
    "namespace": "android_app",
    "package_name": "org.digitalassetlinks.sampleapp",
    "sha256_cert_fingerprints": ["10:39:38:EE:45:37:E5:9E:8E:E7:92:F6"]
  }
  },{
  "relation": ["delegate_permission/common.get_login_creds", "something"],
  "target": {
    "namespace": "web",
    "site": "https://www.google.ru"
  }
  },{
  "relation": ["trash"],
  "target": {
    "namespace": "web",
    "site": "https://www.google.de"
  }
  }
  ])";
  AssetLinkData data;
  EXPECT_TRUE(data.Parse(json));
  EXPECT_THAT(data.includes(), IsEmpty());
  EXPECT_THAT(data.targets(), ElementsAre(GURL("https://www.google.com"),
                                          GURL("https://www.google.ru")));
}

TEST(AssetLinkData, MixedStatements) {
  constexpr char json[] =
      R"([{
  "relation": ["delegate_permission/common.get_login_creds"],
  "target": {
    "namespace": "web",
    "site": "https://www.google.com"
  }
  },{
  "relation": ["unknown", "unknown"],
  "target": {
    "key": "value",
    "key2": 12
  }
  },{
  "include": "https://go/assetlinks.json"
  },{
  "relation": ["delegate_permission/common.get_login_creds"],
  "key": 1234,
  "target": {
    "namespace": "web",
    "site": "https://www.google.de",
    "key": "additional_value"
  }
  }
  ])";
  AssetLinkData data;
  EXPECT_TRUE(data.Parse(json));
  EXPECT_THAT(data.includes(), ElementsAre(GURL("https://go/assetlinks.json")));
  EXPECT_THAT(data.targets(), ElementsAre(GURL("https://www.google.com"),
                                          GURL("https://www.google.de")));
}

}  // namespace
}  // namespace password_manager
