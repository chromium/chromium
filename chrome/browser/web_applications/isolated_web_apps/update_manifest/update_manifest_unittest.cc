// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsTrue;
using testing::Not;
using testing::Optional;
using testing::Property;

TEST(UpdateChannel, DefaultChannel) {
  EXPECT_THAT(UpdateChannel::default_channel(),
              Eq(*UpdateChannel::Create("default")));
  EXPECT_THAT(UpdateChannel::default_channel().ToString(), Eq("default"));
}

using UpdateChannelCreateInvalidTest = testing::TestWithParam<std::string>;

TEST_P(UpdateChannelCreateInvalidTest, Check) {
  EXPECT_THAT(UpdateChannel::Create(GetParam()),
              ErrorIs(Eq(absl::monostate())));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UpdateChannelCreateInvalidTest,
    testing::Values("",
                    // These are a few invalid UTF-8 sequences taken from
                    // `base/strings/string_util_unittest.cc`.
                    "\xED\xA0\x8F",
                    "\xF4\x8F\xBF\xBE",
                    "\xC0\x80"));

using UpdateChannelCreateValidTest = testing::TestWithParam<std::string>;

TEST_P(UpdateChannelCreateValidTest, Check) {
  EXPECT_THAT(
      UpdateChannel::Create(GetParam()),
      ValueIs(Property("ToString", &UpdateChannel::ToString, Eq(GetParam()))));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UpdateChannelCreateValidTest,
    testing::Values(" ",
                    "stable",
                    "default",
                    "123",
                    "  1234 df k;ljh"
                    "a channel name can even have emoji 🚂",
                    "日本"));

TEST(UpdateManifestTest, FailsToParseManifestWithoutKeys) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict()), GURL("https://c.de/um.json"));

  EXPECT_THAT(
      update_manifest,
      ErrorIs(Eq(UpdateManifest::JsonFormatError::kVersionsNotAnArray)));
}

TEST(UpdateManifestTest, FailsToParseManifestWithoutVersions) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set("foo", base::Value::List())),
      GURL("https://c.de/um.json"));

  EXPECT_THAT(
      update_manifest,
      ErrorIs(Eq(UpdateManifest::JsonFormatError::kVersionsNotAnArray)));
}

TEST(UpdateManifestTest, FailsToParseManifestThatIsNotDict) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value("foo")), GURL("https://c.de/um.json"));

  EXPECT_THAT(
      update_manifest,
      ErrorIs(Eq(UpdateManifest::JsonFormatError::kRootNotADictionary)));
}

TEST(UpdateManifestTest, ParsesManifestWithEmptyVersions) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set("versions", base::Value::List())),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(), IsEmpty());
}

TEST(UpdateManifestTest, ParsesManifestWithAdditionalKeys) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict()
                          .Set("foo", base::Value(123))
                          .Set("versions",
                               base::Value::List().Append(
                                   base::Value::Dict()
                                       .Set("version", "1.2.3")
                                       .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre<UpdateManifest::VersionEntry>(
                  {GURL("https://example.com"),
                   base::Version("1.2.3"),
                   {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, ParsesManifestWithVersion) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List().Append(
                              base::Value::Dict()
                                  .Set("version", "1.2.3")
                                  .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre<UpdateManifest::VersionEntry>(
                  {GURL("https://example.com"),
                   base::Version("1.2.3"),
                   {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, ParsesManifestWithRelativeSrc) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List()
                              .Append(base::Value::Dict()
                                          .Set("version", "1.2.3")
                                          .Set("src", "foo/bar"))
                              .Append(base::Value::Dict()
                                          .Set("version", "2.3.4")
                                          .Set("src", "/foo/bar")))),
          GURL("https://c.de/sub/um.json")));

  EXPECT_THAT(
      update_manifest.versions(),
      ElementsAre(
          UpdateManifest::VersionEntry{GURL("https://c.de/sub/foo/bar"),
                                       base::Version("1.2.3"),
                                       {*UpdateChannel::Create("default")}},
          UpdateManifest::VersionEntry{GURL("https://c.de/foo/bar"),
                                       base::Version("2.3.4"),
                                       {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, ParsesManifestWithRelativeSrc2) {
  ASSERT_OK_AND_ASSIGN(auto update_manifest,
                       UpdateManifest::CreateFromJson(
                           base::Value(base::Value::Dict().Set(
                               "versions", base::Value::List().Append(
                                               base::Value::Dict()
                                                   .Set("version", "1.2.3")
                                                   .Set("src", "foo/bar")))),
                           GURL("https://c.de/um")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://c.de/foo/bar"),
                  base::Version("1.2.3"),
                  {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, IgnoresVersionsWithoutUrl) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions",
              base::Value::List()
                  .Append(base::Value::Dict().Set("src", "https://example.com"))
                  .Append(base::Value::Dict()
                              .Set("version", "2.0.0")
                              .Set("src", "https://example2.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example2.com"),
                  base::Version("2.0.0"),
                  {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, IgnoresVersionsWithoutSrc) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions",
              base::Value::List()
                  .Append(base::Value::Dict().Set("version", "1.0.0"))
                  .Append(base::Value::Dict()
                              .Set("version", "2.0.0")
                              .Set("src", "https://example2.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example2.com"),
                  base::Version("2.0.0"),
                  {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, ParsesManifestWithAdditionalVersionKeys) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List().Append(
                              base::Value::Dict()
                                  .Set("foo", 123)
                                  .Set("version", "1.2.3")
                                  .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"),
                  base::Version("1.2.3"),
                  {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, ParsesManifestWithVersionChannels) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions",
              base::Value::List()
                  .Append(base::Value::Dict()
                              .Set("channels",
                                   base::Value::List().Append("beta").Append(
                                       "stable"))
                              .Set("version", "1.2.3")
                              .Set("src", "https://example.com"))
                  .Append(base::Value::Dict()
                              .Set("channels", base::Value::List())
                              .Set("version", "1.2.4")
                              .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));
  EXPECT_THAT(
      update_manifest.versions(),
      ElementsAre(
          UpdateManifest::VersionEntry{GURL("https://example.com"),
                                       base::Version("1.2.3"),
                                       {*UpdateChannel::Create("beta"),
                                        *UpdateChannel::Create("stable")}},
          UpdateManifest::VersionEntry{
              GURL("https://example.com"), base::Version("1.2.4"),
              // If the Update Manifest contains "channels: []", then we
              // do _not_ automatically add "default" to it.
              /*channels=*/{}}));
}

TEST(UpdateManifestTest, IgnoresChannelOrder) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest1,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions",
              base::Value::List().Append(
                  base::Value::Dict()
                      .Set("channels",
                           base::Value::List().Append("beta").Append("stable"))
                      .Set("version", "1.2.3")
                      .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest2,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions",
              base::Value::List().Append(
                  base::Value::Dict()
                      .Set("channels",
                           base::Value::List().Append("stable").Append("beta"))
                      .Set("version", "1.2.3")
                      .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest1.versions(), Eq(update_manifest2.versions()));
  EXPECT_THAT(
      update_manifest1.versions(),
      ElementsAre(UpdateManifest::VersionEntry{
          GURL("https://example.com"),
          base::Version("1.2.3"),
          // Order should not matter here, because it is a set.
          {*UpdateChannel::Create("stable"), *UpdateChannel::Create("beta")}}));
}

TEST(UpdateManifestTest, DoesNotAllowEmptyChannels) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions",
              base::Value::List().Append(
                  base::Value::Dict()
                      .Set("channels",
                           base::Value::List().Append("").Append("stable"))
                      .Set("version", "1.2.3")
                      .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));
  EXPECT_THAT(update_manifest.versions(), IsEmpty());
}

TEST(UpdateManifestTest, ParsesManifestWithMultipleVersions) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List()
                              .Append(base::Value::Dict()
                                          .Set("version", "1.2.3")
                                          .Set("src", "https://example.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "3.0.0")
                                          .Set("src", "http://localhost")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(
      update_manifest.versions(),
      ElementsAre(
          UpdateManifest::VersionEntry{GURL("https://example.com"),
                                       base::Version("1.2.3"),
                                       {*UpdateChannel::Create("default")}},
          UpdateManifest::VersionEntry{GURL("http://localhost"),
                                       base::Version("3.0.0"),
                                       {*UpdateChannel::Create("default")}}));
}

TEST(UpdateManifestTest, OverwritesRepeatedEntriesWithSameVersion) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List()
                              .Append(base::Value::Dict()
                                          .Set("version", "3.0.0")
                                          .Set("src", "https://v3-1.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "3.0.0")
                                          .Set("src", "https://v3-2.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "5.0.0")
                                          .Set("src", "https://v5-1.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "3.0.0")
                                          .Set("src", "https://v3-3.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "5.0.0")
                                          .Set("src", "https://v5-2.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(
      update_manifest.versions(),
      ElementsAre(
          UpdateManifest::VersionEntry{GURL("https://v3-3.com"),
                                       base::Version("3.0.0"),
                                       {*UpdateChannel::Create("default")}},
          UpdateManifest::VersionEntry{GURL("https://v5-2.com"),
                                       base::Version("5.0.0"),
                                       {*UpdateChannel::Create("default")}}));
}

class UpdateManifestValidVersionTest
    : public testing::TestWithParam<std::string> {};

TEST_P(UpdateManifestValidVersionTest, ParsesValidVersion) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List().Append(
                              base::Value::Dict()
                                  .Set("version", GetParam())
                                  .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"),
                  base::Version(GetParam()),
                  {*UpdateChannel::Create("default")}}));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         UpdateManifestValidVersionTest,
                         ::testing::Values("1.2.3", "10.20.30", "0.0.0"));

using UpdateManifestInvalidVersionTest = testing::TestWithParam<std::string>;

TEST_P(UpdateManifestInvalidVersionTest, IgnoresEntriesWithInvalidVersions) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List()
                              .Append(base::Value::Dict()
                                          .Set("version", GetParam())
                                          .Set("src", "https://example.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "99.99.99")
                                          .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"),
                  base::Version("99.99.99"),
                  {*UpdateChannel::Create("default")}}));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         UpdateManifestInvalidVersionTest,
                         ::testing::Values("abc", "1.3.4-beta", ""));

using UpdateManifestValidSrcTest = testing::TestWithParam<std::string>;

TEST_P(UpdateManifestValidSrcTest, ParsesValidSrc) {
  ASSERT_OK_AND_ASSIGN(auto update_manifest,
                       UpdateManifest::CreateFromJson(
                           base::Value(base::Value::Dict().Set(
                               "versions", base::Value::List().Append(
                                               base::Value::Dict()
                                                   .Set("version", "1.0.0")
                                                   .Set("src", GetParam())))),
                           GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL(GetParam()),
                  base::Version("1.0.0"),
                  {*UpdateChannel::Create("default")}}));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         UpdateManifestValidSrcTest,
                         ::testing::Values("https://localhost",
                                           "http://localhost",
                                           "https://example.com",
                                           "https://example.com:1234"));

using UpdateManifestInvalidSrcTest = testing::TestWithParam<std::string>;

TEST_P(UpdateManifestInvalidSrcTest, IgnoresEntriesWithInvalidSrc) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List()
                              .Append(base::Value::Dict()
                                          .Set("version", "1.0.0")
                                          .Set("src", GetParam()))
                              .Append(base::Value::Dict()
                                          .Set("version", "99.99.99")
                                          .Set("src", "https://example.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(update_manifest.versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"),
                  base::Version("99.99.99"),
                  {*UpdateChannel::Create("default")}}));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         UpdateManifestInvalidSrcTest,
                         ::testing::Values("http://example.com",
                                           "foo:123",
                                           "file:///test.swbn",
                                           "about:blank",
                                           "",
                                           "isolated-app://foo"));

class UpdateManifestSecureOriginAllowlistTest : public ::testing::Test {
 protected:
  void TearDown() override {
    network::SecureOriginAllowlist::GetInstance().ResetForTesting();
  }
};

TEST_F(UpdateManifestSecureOriginAllowlistTest, CanSetHttpOriginsAsTrusted) {
  auto update_manifest_json = base::Value(base::Value::Dict().Set(
      "versions",
      base::Value::List().Append(base::Value::Dict()
                                     .Set("version", "1.0.0")
                                     .Set("src", "http://example.com"))));

  {
    ASSERT_OK_AND_ASSIGN(
        auto update_manifest,
        UpdateManifest::CreateFromJson(update_manifest_json,
                                       GURL("https://c.de/um.json")));
    EXPECT_THAT(update_manifest.versions(), IsEmpty());
  }

  std::vector<std::string> rejected_patterns;
  network::SecureOriginAllowlist::GetInstance().SetAuxiliaryAllowlist(
      "http://example.com", &rejected_patterns);
  EXPECT_THAT(rejected_patterns, IsEmpty());

  {
    ASSERT_OK_AND_ASSIGN(
        auto update_manifest,
        UpdateManifest::CreateFromJson(update_manifest_json,
                                       GURL("https://c.de/um.json")));
    EXPECT_THAT(update_manifest.versions(),
                ElementsAre(UpdateManifest::VersionEntry{
                    GURL("http://example.com"),
                    base::Version("1.0.0"),
                    {*UpdateChannel::Create("default")}}));
  }
}

TEST(GetLatestVersionTest, CalculatesLatestVersionCorrectly) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions", base::Value::List()
                              .Append(base::Value::Dict()
                                          .Set("version", "3.99.123")
                                          .Set("src", "https://v3.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "5.6.0")
                                          .Set("src", "https://v5.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "10.3.0")
                                          .Set("src", "https://v10.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "10.11.0")
                                          .Set("src", "https://v10.com"))
                              .Append(base::Value::Dict()
                                          .Set("version", "4.5.0")
                                          .Set("src", "https://v4.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(
      update_manifest.GetLatestVersion(*UpdateChannel::Create("default")),
      Optional(Eq(
          UpdateManifest::VersionEntry{GURL("https://v10.com"),
                                       base::Version("10.11.0"),
                                       {*UpdateChannel::Create("default")}})));

  EXPECT_THAT(
      update_manifest.GetLatestVersion(*UpdateChannel::Create("non-existing")),
      Eq(std::nullopt));
}

TEST(GetLatestVersionTest, CalculatesLatestVersionForChannel) {
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(
          base::Value(base::Value::Dict().Set(
              "versions",
              base::Value::List()
                  .Append(base::Value::Dict()
                              .Set("version", "3.99.123")
                              .Set("src", "https://v3.com")
                              .Set("channels",
                                   base::Value::List().Append("default")))
                  .Append(base::Value::Dict()
                              .Set("version", "5.6.0")
                              .Set("src", "https://v5.com")
                              .Set("channels",
                                   base::Value::List().Append("default").Append(
                                       "beta")))
                  .Append(base::Value::Dict()
                              .Set("version", "10.3.0")
                              .Set("src", "https://v10.com"))
                  .Append(base::Value::Dict()
                              .Set("version", "10.11.0")
                              .Set("src", "https://v10.com"))
                  .Append(base::Value::Dict()
                              .Set("version", "4.5.0")
                              .Set("src", "https://v4.com")))),
          GURL("https://c.de/um.json")));

  EXPECT_THAT(
      update_manifest.GetLatestVersion(*UpdateChannel::Create("default")),
      Optional(Eq(
          UpdateManifest::VersionEntry{GURL("https://v10.com"),
                                       base::Version("10.11.0"),
                                       {*UpdateChannel::Create("default")}})));

  EXPECT_THAT(
      update_manifest.GetLatestVersion(*UpdateChannel::Create("non-existing")),
      Eq(std::nullopt));
}

TEST(UpdateManifestParsesChannelMetadataTest, ChannelsMissing) {
  ASSERT_OK_AND_ASSIGN(base::Value json,
                       base::JSONReader::ReadAndReturnValueWithError(R"({
    "versions": []
  })"));
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(json, GURL("https://c.de/um.json")));

  EXPECT_THAT(
      update_manifest.GetChannelMetadata(UpdateChannel::default_channel()),
      Eq(UpdateManifest::ChannelMetadata(
          /*update_channel=*/UpdateChannel::default_channel(),
          /*display_name=*/std::nullopt)));

  ASSERT_OK_AND_ASSIGN(auto channel, UpdateChannel::Create("some_name"));
  EXPECT_THAT(update_manifest.GetChannelMetadata(channel),
              Eq(UpdateManifest::ChannelMetadata(
                  /*update_channel=*/channel,
                  /*display_name=*/std::nullopt)));
}

TEST(UpdateManifestParsesChannelMetadataTest, EmptyChannelMetadata) {
  ASSERT_OK_AND_ASSIGN(base::Value json,
                       base::JSONReader::ReadAndReturnValueWithError(R"({
    "channels": {},
    "versions": []
  })"));
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(json, GURL("https://c.de/um.json")));

  EXPECT_THAT(
      update_manifest.GetChannelMetadata(UpdateChannel::default_channel()),
      Eq(UpdateManifest::ChannelMetadata(
          /*update_channel=*/UpdateChannel::default_channel(),
          /*display_name=*/std::nullopt)));

  ASSERT_OK_AND_ASSIGN(auto channel, UpdateChannel::Create("some_name"));
  EXPECT_THAT(update_manifest.GetChannelMetadata(channel),
              Eq(UpdateManifest::ChannelMetadata(
                  /*update_channel=*/channel,
                  /*display_name=*/std::nullopt)));
}

TEST(UpdateManifestParsesChannelMetadataTest, ChannelsNotADict) {
  ASSERT_OK_AND_ASSIGN(base::Value json,
                       base::JSONReader::ReadAndReturnValueWithError(R"({
    "channels": [],
    "versions": []
  })"));
  EXPECT_THAT(
      UpdateManifest::CreateFromJson(json, GURL("https://c.de/um.json")),
      ErrorIs(Eq(UpdateManifest::JsonFormatError::kChannelsNotADictionary)));
}

TEST(UpdateManifestParsesChannelMetadataTest, ChannelNotADict) {
  ASSERT_OK_AND_ASSIGN(base::Value json,
                       base::JSONReader::ReadAndReturnValueWithError(R"({
    "channels": {
      "default": []
    },
    "versions": []
  })"));
  EXPECT_THAT(
      UpdateManifest::CreateFromJson(json, GURL("https://c.de/um.json")),
      ErrorIs(Eq(UpdateManifest::JsonFormatError::kChannelNotADictionary)));
}

TEST(UpdateManifestParsesChannelMetadataTest, ChannelMetadataWithoutName) {
  ASSERT_OK_AND_ASSIGN(base::Value json,
                       base::JSONReader::ReadAndReturnValueWithError(R"({
    "channels": {
      "default": {}
    },
    "versions": []
  })"));
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(json, GURL("https://c.de/um.json")));

  auto channel_metadata =
      update_manifest.GetChannelMetadata(UpdateChannel::default_channel());
  EXPECT_THAT(channel_metadata,
              Eq(UpdateManifest::ChannelMetadata(
                  /*update_channel=*/UpdateChannel::default_channel(),
                  /*display_name=*/std::nullopt)));
  EXPECT_THAT(channel_metadata.GetDisplayName(), Eq("default"));
}

TEST(UpdateManifestParsesChannelMetadataTest,
     ChannelMetadataWithAdditionalField) {
  ASSERT_OK_AND_ASSIGN(base::Value json,
                       base::JSONReader::ReadAndReturnValueWithError(R"({
    "channels": {
      "default": {
        "flubber": "blubber"
      }
    },
    "versions": []
  })"));
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(json, GURL("https://c.de/um.json")));

  EXPECT_THAT(
      update_manifest.GetChannelMetadata(UpdateChannel::default_channel()),
      Eq(UpdateManifest::ChannelMetadata(
          /*update_channel=*/UpdateChannel::default_channel(),
          /*display_name=*/std::nullopt)));
}

TEST(UpdateManifestParsesChannelMetadataTest, ChannelName) {
  ASSERT_OK_AND_ASSIGN(base::Value json,
                       base::JSONReader::ReadAndReturnValueWithError(R"({
    "channels": {
      "default": {
        "name": "default channel"
      },
      "some_name": {
        "name": "some channel"
      },
      "another_name": {
      }
    },
    "versions" : []
  })"));
  ASSERT_OK_AND_ASSIGN(
      auto update_manifest,
      UpdateManifest::CreateFromJson(json, GURL("https://c.de/um.json")));

  {
    auto channel_metadata =
        update_manifest.GetChannelMetadata(UpdateChannel::default_channel());
    EXPECT_THAT(channel_metadata,
                Eq(UpdateManifest::ChannelMetadata(
                    /*update_channel=*/UpdateChannel::default_channel(),
                    /*display_name=*/"default channel")));
    EXPECT_THAT(channel_metadata.GetDisplayName(), Eq("default channel"));
  }

  {
    ASSERT_OK_AND_ASSIGN(auto channel, UpdateChannel::Create("some_name"));
    auto channel_metadata = update_manifest.GetChannelMetadata(channel);
    EXPECT_THAT(channel_metadata, Eq(UpdateManifest::ChannelMetadata(
                                      /*update_channel=*/channel,
                                      /*display_name=*/"some channel")));
    EXPECT_THAT(channel_metadata.GetDisplayName(), Eq("some channel"));
  }

  {
    ASSERT_OK_AND_ASSIGN(auto channel, UpdateChannel::Create("another_name"));
    auto channel_metadata = update_manifest.GetChannelMetadata(channel);
    EXPECT_THAT(channel_metadata, Eq(UpdateManifest::ChannelMetadata(
                                      /*update_channel=*/channel,
                                      /*display_name=*/std::nullopt)));
    EXPECT_THAT(channel_metadata.GetDisplayName(), Eq("another_name"));
  }
}

}  // namespace
}  // namespace web_app
