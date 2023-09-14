// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using base::test::HasValue;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsTrue;
using testing::Not;

TEST(UpdateManifestTest, FailsToParseManifestWithoutKeys) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict()), GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsFalse());
  EXPECT_THAT(update_manifest.error(),
              Eq(UpdateManifest::JsonFormatError::kVersionsNotAnArray));
}

TEST(UpdateManifestTest, FailsToParseManifestWithoutVersions) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set("foo", base::Value::List())),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsFalse());
  EXPECT_THAT(update_manifest.error(),
              Eq(UpdateManifest::JsonFormatError::kVersionsNotAnArray));
}

TEST(UpdateManifestTest, FailsToParseManifestThatIsNotDict) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value("foo")), GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsFalse());
  EXPECT_THAT(update_manifest.error(),
              Eq(UpdateManifest::JsonFormatError::kRootNotADictionary));
}

TEST(UpdateManifestTest, ParsesManifestWithEmptyVersions) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set("versions", base::Value::List())),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsFalse());
  EXPECT_THAT(update_manifest.error(),
              Eq(UpdateManifest::JsonFormatError::kNoApplicableVersion));
}

TEST(UpdateManifestTest, ParsesManifestWithAdditionalKeys) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(
          base::Value::Dict()
              .Set("foo", base::Value(123))
              .Set("versions", base::Value::List().Append(
                                   base::Value::Dict()
                                       .Set("version", "1.2.3")
                                       .Set("src", "https://example.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre<UpdateManifest::VersionEntry>(
                  {GURL("https://example.com"), base::Version("1.2.3")}));
}

TEST(UpdateManifestTest, ParsesManifestWithVersion) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions",
          base::Value::List().Append(base::Value::Dict()
                                         .Set("version", "1.2.3")
                                         .Set("src", "https://example.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre<UpdateManifest::VersionEntry>(
                  {GURL("https://example.com"), base::Version("1.2.3")}));
}

TEST(UpdateManifestTest, ParsesManifestWithRelativeSrc) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions", base::Value::List()
                          .Append(base::Value::Dict()
                                      .Set("version", "1.2.3")
                                      .Set("src", "foo/bar"))
                          .Append(base::Value::Dict()
                                      .Set("version", "2.3.4")
                                      .Set("src", "/foo/bar")))),
      GURL("https://c.de/sub/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(
      update_manifest->versions(),
      ElementsAre(UpdateManifest::VersionEntry{GURL("https://c.de/sub/foo/bar"),
                                               base::Version("1.2.3")},
                  UpdateManifest::VersionEntry{GURL("https://c.de/foo/bar"),
                                               base::Version("2.3.4")}));
}

TEST(UpdateManifestTest, ParsesManifestWithRelativeSrc2) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions", base::Value::List().Append(base::Value::Dict()
                                                     .Set("version", "1.2.3")
                                                     .Set("src", "foo/bar")))),
      GURL("https://c.de/um"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://c.de/foo/bar"), base::Version("1.2.3")}));
}

TEST(UpdateManifestTest, IgnoresVersionsWithoutUrl) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions",
          base::Value::List()
              .Append(base::Value::Dict().Set("src", "https://example.com"))
              .Append(base::Value::Dict()
                          .Set("version", "2.0.0")
                          .Set("src", "https://example2.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example2.com"), base::Version("2.0.0")}));
}

TEST(UpdateManifestTest, IgnoresVersionsWithoutSrc) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions", base::Value::List()
                          .Append(base::Value::Dict().Set("version", "1.0.0"))
                          .Append(base::Value::Dict()
                                      .Set("version", "2.0.0")
                                      .Set("src", "https://example2.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example2.com"), base::Version("2.0.0")}));
}

TEST(UpdateManifestTest, ParsesManifestWithAdditionalVersionKeys) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions",
          base::Value::List().Append(base::Value::Dict()
                                         .Set("foo", 123)
                                         .Set("version", "1.2.3")
                                         .Set("src", "https://example.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"), base::Version("1.2.3")}));
}

TEST(UpdateManifestTest, ParsesManifestWithMultipleVersions) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions", base::Value::List()
                          .Append(base::Value::Dict()
                                      .Set("version", "1.2.3")
                                      .Set("src", "https://example.com"))
                          .Append(base::Value::Dict()
                                      .Set("version", "3.0.0")
                                      .Set("src", "http://localhost")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(
      update_manifest->versions(),
      ElementsAre(UpdateManifest::VersionEntry{GURL("https://example.com"),
                                               base::Version("1.2.3")},
                  UpdateManifest::VersionEntry{GURL("http://localhost"),
                                               base::Version("3.0.0")}));
}

TEST(UpdateManifestTest, OverwritesRepeatedEntriesWithSameVersion) {
  auto update_manifest = UpdateManifest::CreateFromJson(
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
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(
      update_manifest->versions(),
      ElementsAre(UpdateManifest::VersionEntry{GURL("https://v3-3.com"),
                                               base::Version("3.0.0")},
                  UpdateManifest::VersionEntry{GURL("https://v5-2.com"),
                                               base::Version("5.0.0")}));
}

class UpdateManifestValidVersionTest
    : public testing::TestWithParam<std::string> {};

TEST_P(UpdateManifestValidVersionTest, ParsesValidVersion) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions",
          base::Value::List().Append(base::Value::Dict()
                                         .Set("version", GetParam())
                                         .Set("src", "https://example.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"), base::Version(GetParam())}));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         UpdateManifestValidVersionTest,
                         ::testing::Values("1.2.3", "10.20.30", "0.0.0"));

class UpdateManifestInvalidVersionTest
    : public testing::TestWithParam<std::string> {};

TEST_P(UpdateManifestInvalidVersionTest, IgnoresEntriesWithInvalidVersions) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions", base::Value::List()
                          .Append(base::Value::Dict()
                                      .Set("version", GetParam())
                                      .Set("src", "https://example.com"))
                          .Append(base::Value::Dict()
                                      .Set("version", "99.99.99")
                                      .Set("src", "https://example.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"), base::Version("99.99.99")}));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         UpdateManifestInvalidVersionTest,
                         ::testing::Values("abc", "1.3.4-beta", ""));

class UpdateManifestValidSrcTest : public testing::TestWithParam<std::string> {
};

TEST_P(UpdateManifestValidSrcTest, ParsesValidSrc) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions", base::Value::List().Append(base::Value::Dict()
                                                     .Set("version", "1.0.0")
                                                     .Set("src", GetParam())))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL(GetParam()), base::Version("1.0.0")}));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         UpdateManifestValidSrcTest,
                         ::testing::Values("https://localhost",
                                           "http://localhost",
                                           "https://example.com",
                                           "https://example.com:1234"));

class UpdateManifestInvalidSrcTest
    : public testing::TestWithParam<std::string> {};

TEST_P(UpdateManifestInvalidSrcTest, IgnoresEntriesWithInvalidSrc) {
  auto update_manifest = UpdateManifest::CreateFromJson(
      base::Value(base::Value::Dict().Set(
          "versions", base::Value::List()
                          .Append(base::Value::Dict()
                                      .Set("version", "1.0.0")
                                      .Set("src", GetParam()))
                          .Append(base::Value::Dict()
                                      .Set("version", "99.99.99")
                                      .Set("src", "https://example.com")))),
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(update_manifest->versions(),
              ElementsAre(UpdateManifest::VersionEntry{
                  GURL("https://example.com"), base::Version("99.99.99")}));
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
    auto update_manifest = UpdateManifest::CreateFromJson(
        update_manifest_json, GURL("https://c.de/um.json"));
    EXPECT_THAT(update_manifest, Not(HasValue()));
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
                    GURL("http://example.com"), base::Version("1.0.0")}));
  }
}

TEST(GetLatestVersionEntryTest, CalculatesLatestVersionCorrectly) {
  auto update_manifest = UpdateManifest::CreateFromJson(
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
      GURL("https://c.de/um.json"));

  ASSERT_THAT(update_manifest.has_value(), IsTrue());
  EXPECT_THAT(GetLatestVersionEntry(*update_manifest),
              Eq(UpdateManifest::VersionEntry{GURL("https://v10.com"),
                                              base::Version("10.11.0")}));
}

}  // namespace
}  // namespace web_app
