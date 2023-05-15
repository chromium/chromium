// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
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
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::StartsWith;
using ::testing::Values;

constexpr char kValidIsolatedWebAppUrl[] =
    "isolated-app://"
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
    "?foo=bar#baz";
}  // namespace

using IsolatedWebAppUrlInfoTest = ::testing::Test;

TEST_F(IsolatedWebAppUrlInfoTest, CreateSucceedsWithValidUrl) {
  EXPECT_TRUE(
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedWebAppUrl)).has_value());
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithInvalidScheme) {
  GURL gurl(
      "https://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(gurl);
  ASSERT_FALSE(url_info.has_value());
  EXPECT_THAT(url_info.error(), StartsWith("The URL scheme must be"));
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithInvalidUrl) {
  GURL gurl("aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(gurl);
  ASSERT_FALSE(url_info.has_value());
  EXPECT_THAT(url_info.error(), Eq("Invalid URL"));
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithSubdomain) {
  GURL gurl(
      "isolated-app://"
      "foo.aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(gurl);
  ASSERT_FALSE(url_info.has_value());
  EXPECT_THAT(url_info.error(),
              StartsWith("The host of isolated-app:// URLs must be a valid"));
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithBadHostname) {
  GURL gurl(
      "isolated-app://"
      "ßerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(gurl);
  ASSERT_FALSE(url_info.has_value());
  EXPECT_THAT(url_info.error(),
              StartsWith("The host of isolated-app:// URLs must be a valid"));
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

  auto expected_config = content::StoragePartitionConfig::Create(
      &testing_profile,
      /*partition_domain=*/
      "iwa-aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
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
       GetIsolatedWebAppUrlInfoWhenInstalledBundleSucceeds) {
  IsolatedWebAppLocation location = InstalledBundle{};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      location, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("is not implemented"));
}

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoWhenDevModeBundleSucceeds) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(base::FilePath::FromASCII("test-0.swbn"));
  TestSignedWebBundle bundle = BuildDefaultTestSignedWebBundle();
  ASSERT_TRUE(base::WriteFile(path, bundle.data));

  IsolatedWebAppLocation location = DevModeBundle{.path = path};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;
  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      location, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().web_bundle_id(), bundle.id);
}

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoWhenDevModeBundleFailsWhenFileNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().Append(
      base::FilePath::FromASCII("file_not_exist.swbn"));
  IsolatedWebAppLocation location = DevModeBundle{.path = path};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      location, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              HasSubstr("Failed to read the integrity block of the signed web "
                        "bundle: FILE_ERROR_NOT_FOUND"));
}

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoWhenDevModeBundleFailsWhenInvalidFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(base::FilePath::FromASCII("invalid_file.swbn"));
  ASSERT_TRUE(
      base::WriteFile(path, "clearly, this is not a valid signed web bundle"));
  IsolatedWebAppLocation location = DevModeBundle{.path = path};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      location, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              HasSubstr("Failed to read the integrity block of the signed web "
                        "bundle: Wrong array size or magic bytes."));
}

TEST_F(IsolatedWebAppUrlInfoFromIsolatedWebAppLocationTest,
       GetIsolatedWebAppUrlInfoSucceedsWhenDevModeProxy) {
  IsolatedWebAppLocation location = DevModeProxy{};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      location, test_future.GetCallback());
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
