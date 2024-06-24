// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {
using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Property;
using ::testing::StartsWith;
using ::testing::Values;

constexpr char kValidIsolatedWebAppUrl[] =
    "isolated-app://"
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
    "?foo=bar#baz";
}  // namespace

using IsolatedWebAppUrlInfoTest = ::testing::Test;

TEST_F(IsolatedWebAppUrlInfoTest, CreateSucceedsWithValidUrl) {
  EXPECT_THAT(IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedWebAppUrl)),
              HasValue());
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithInvalidScheme) {
  GURL gurl(
      "https://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  EXPECT_THAT(IsolatedWebAppUrlInfo::Create(gurl),
              ErrorIs(StartsWith("The URL scheme must be")));
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithInvalidUrl) {
  GURL gurl("aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  EXPECT_THAT(IsolatedWebAppUrlInfo::Create(gurl), ErrorIs("Invalid URL"));
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithSubdomain) {
  GURL gurl(
      "isolated-app://"
      "foo.aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  EXPECT_THAT(
      IsolatedWebAppUrlInfo::Create(gurl),
      ErrorIs(StartsWith("The host of isolated-app:// URLs must be a valid")));
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithBadHostname) {
  GURL gurl(
      "isolated-app://"
      "ßerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  EXPECT_THAT(
      IsolatedWebAppUrlInfo::Create(gurl),
      ErrorIs(StartsWith("The host of isolated-app:// URLs must be a valid")));
}

TEST_F(IsolatedWebAppUrlInfoTest, OriginIsCorrect) {
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedWebAppUrl));

  EXPECT_THAT(url_info->origin().Serialize(),
              Eq("isolated-app://"
                 "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"));
}

TEST_F(IsolatedWebAppUrlInfoTest, AppIdIsHashedOrigin) {
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedWebAppUrl));

  EXPECT_THAT(url_info->app_id(), Eq("ckmbeioemjmabdoddhjadagkjknpeigi"));
}

TEST_F(IsolatedWebAppUrlInfoTest, WebBundleIdIsCorrect) {
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedWebAppUrl));

  EXPECT_THAT(url_info->web_bundle_id().id(),
              Eq("aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"));
}

TEST_F(IsolatedWebAppUrlInfoTest, GetStoragePartitionConfigForControlledFrame) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile testing_profile;

  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedWebAppUrl));
  content::StoragePartitionConfig iwa_config =
      url_info->storage_partition_config(&testing_profile);
  content::StoragePartitionConfig frame_config =
      url_info->GetStoragePartitionConfigForControlledFrame(
          &testing_profile, "name", /*in_memory=*/false);

  EXPECT_THAT(frame_config.partition_domain(),
              Eq(iwa_config.partition_domain()));
  EXPECT_THAT(frame_config.partition_name(), Eq("name"));
  EXPECT_FALSE(frame_config.in_memory());
}

TEST_F(IsolatedWebAppUrlInfoTest, StoragePartitionConfigUsesOrigin) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile testing_profile;

  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedWebAppUrl));

  // "ih5acGGEiRXrgomjVcGuM1lp4cp+dagupnpwXmiyoV0s=" is the base64 encoding of
  // the first 6 bytes of sha256 of the App ID
  // ("ckmbeioemjmabdoddhjadagkjknpeigi").
  auto expected_config = content::StoragePartitionConfig::Create(
      &testing_profile,
      /*partition_domain=*/
      "ih5acGGEiRXrgomjVcGuM1lp4cp+dagupnpwXmiyoV0s=",
      /*partition_name=*/"",
      /*in_memory=*/false);
  EXPECT_THAT(url_info->storage_partition_config(&testing_profile),
              Eq(expected_config));
}

class IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest
    : public ::testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoWhenBundleSucceeds) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(base::FilePath::FromASCII("test-0.swbn"));
  TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault();
  ASSERT_TRUE(base::WriteFile(path, bundle.data));

  IwaSource source{IwaSourceBundle(path)};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;
  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
      source, test_future.GetCallback());
  EXPECT_THAT(
      test_future.Get(),
      ValueIs(Property(&IsolatedWebAppUrlInfo::web_bundle_id, bundle.id)));
}

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoWhenBundleFailsWhenFileNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().Append(
      base::FilePath::FromASCII("file_not_exist.swbn"));
  IwaSource source{IwaSourceBundle(path)};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
      source, test_future.GetCallback());
  EXPECT_THAT(
      test_future.Get(),
      ErrorIs(HasSubstr("Failed to read the integrity block of the signed web "
                        "bundle: FILE_ERROR_NOT_FOUND")));
}

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoWhenBundleFailsWhenInvalidFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(base::FilePath::FromASCII("invalid_file.swbn"));
  ASSERT_TRUE(
      base::WriteFile(path, "clearly, this is not a valid signed web bundle"));
  IwaSource source{IwaSourceBundle(path)};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
      source, test_future.GetCallback());
  EXPECT_THAT(
      test_future.Get(),
      ErrorIs(HasSubstr(
          "Failed to read the integrity block of the signed web "
          "bundle: Error reading the integrity block array structure.")));
}

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoSucceedsWhenProxy) {
  IwaSource source(IwaSourceProxy{url::Origin()});
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
      source, test_future.GetCallback());
  EXPECT_TRUE(test_future.Get().has_value());
}

class IsolatedWebAppGURLConversionTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(IsolatedWebAppGURLConversionTest, RemovesInvalidPartsFromUrls) {
  // GURL automatically removes port and credentials, and converts
  // `isolated-app:foo` to `isolated-app://foo`. This test is here to verify
  // that and therefore make sure that the `DCHECK` inside
  // `ParseSignedWebBundleId` will never actually trigger as long as this test
  // succeeds.
  GURL gurl(GetParam().first);
  EXPECT_TRUE(gurl.IsStandard());
  EXPECT_EQ(gurl.spec(), GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppGURLConversionTest,
    Values(
        std::make_pair(kValidIsolatedWebAppUrl, kValidIsolatedWebAppUrl),
        // credentials
        std::make_pair(
            "isolated-app://"
            "foo:bar@aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
            "?foo=bar#baz",
            kValidIsolatedWebAppUrl),
        // explicit port
        std::make_pair(
            "isolated-app://"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic:123/"
            "?foo=bar#baz",
            kValidIsolatedWebAppUrl),
        // missing `//`
        std::make_pair(
            "isolated-app:"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
            "?foo=bar#baz",
            kValidIsolatedWebAppUrl)));

}  // namespace web_app
