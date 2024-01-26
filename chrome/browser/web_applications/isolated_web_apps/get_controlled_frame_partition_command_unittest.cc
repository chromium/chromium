// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_controlled_frame_partition_command.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "url/gurl.h"

using ::testing::UnorderedElementsAre;

namespace web_app {

class GetControlledFramePartitionCommandTest : public WebAppTest {
 public:
  GetControlledFramePartitionCommandTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

  void SetUp() override {
    WebAppTest::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  content::StoragePartitionConfig RunCommand(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name,
      bool in_memory = false,
      const base::Location location = FROM_HERE) {
    base::test::TestFuture<std::optional<content::StoragePartitionConfig>>
        future;
    provider().scheduler().ScheduleCallbackWithResult(
        "GetControlledFramePartition", AppLockDescription(url_info.app_id()),
        base::BindOnce(&GetControlledFramePartitionWithLock, profile(),
                       url_info, partition_name, in_memory),
        future.GetCallback(), /*arg_for_shutdown=*/
        std::optional<content::StoragePartitionConfig>(std::nullopt), location);
    return future.Get().value();
  }

  IsolatedWebAppUrlInfo InstallIsolatedWebApp(const GURL& url) {
    AddDummyIsolatedAppToRegistry(profile(), url, "IWA Name");
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
        IsolatedWebAppUrlInfo::Create(url);
    CHECK(url_info.has_value());
    return *url_info;
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GetControlledFramePartitionCommandTest, CanRegisterPartition) {
  const GURL app_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(app_url);

  content::StoragePartitionConfig config = RunCommand(url_info, "name1");

  content::StoragePartitionConfig expected_config =
      url_info.GetStoragePartitionConfigForControlledFrame(profile(), "name1",
                                                           /*in_memory=*/false);
  EXPECT_EQ(config, expected_config);

  std::vector<content::StoragePartitionConfig> storage_partitions =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(url_info.app_id());
  EXPECT_THAT(storage_partitions,
              UnorderedElementsAre(url_info.storage_partition_config(profile()),
                                   expected_config));
}

TEST_F(GetControlledFramePartitionCommandTest, CanRegisterMultiplePartitions) {
  const GURL app_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(app_url);

  content::StoragePartitionConfig config1 = RunCommand(url_info, "name1");
  content::StoragePartitionConfig config2 = RunCommand(url_info, "name2");

  content::StoragePartitionConfig expected_config1 =
      url_info.GetStoragePartitionConfigForControlledFrame(profile(), "name1",
                                                           /*in_memory=*/false);
  content::StoragePartitionConfig expected_config2 =
      url_info.GetStoragePartitionConfigForControlledFrame(profile(), "name2",
                                                           /*in_memory=*/false);
  EXPECT_EQ(config1, expected_config1);
  EXPECT_EQ(config2, expected_config2);

  std::vector<content::StoragePartitionConfig> storage_partitions =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(url_info.app_id());
  EXPECT_THAT(storage_partitions,
              UnorderedElementsAre(url_info.storage_partition_config(profile()),
                                   expected_config1, expected_config2));
}

TEST_F(GetControlledFramePartitionCommandTest, DuplicatePartitionsIgnored) {
  const GURL app_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(app_url);
  content::StoragePartitionConfig config1 = RunCommand(url_info, "name1");
  content::StoragePartitionConfig config2 = RunCommand(url_info, "name1");

  content::StoragePartitionConfig expected_config =
      url_info.GetStoragePartitionConfigForControlledFrame(profile(), "name1",
                                                           /*in_memory=*/false);
  EXPECT_EQ(config1, config2);
  EXPECT_EQ(config1, expected_config);

  std::vector<content::StoragePartitionConfig> storage_partitions =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(url_info.app_id());
  EXPECT_THAT(storage_partitions,
              UnorderedElementsAre(url_info.storage_partition_config(profile()),
                                   expected_config));
}

TEST_F(GetControlledFramePartitionCommandTest, InMemoryPartitionsIsSaved) {
  const GURL app_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(app_url);

  content::StoragePartitionConfig config =
      RunCommand(url_info, "name1", /*in_memory=*/true);

  content::StoragePartitionConfig expected_config =
      url_info.GetStoragePartitionConfigForControlledFrame(profile(), "name1",
                                                           /*in_memory=*/true);
  EXPECT_EQ(config, expected_config);

  std::vector<content::StoragePartitionConfig> storage_partitions =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(url_info.app_id());

  EXPECT_EQ(2UL, storage_partitions.size());
  EXPECT_THAT(
      storage_partitions,
      UnorderedElementsAre(expected_config,
                           url_info.storage_partition_config(profile())));
}

TEST_F(GetControlledFramePartitionCommandTest, CorrectWithDifferentApps) {
  // Set up IWA 1.
  const GURL iwa_1_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  const std::string expected_partition_domain_1 =
      "i1kr80qqyjuuVC4UFPN7ovBngVoA2HbXGtTXtmQn6/H4=";
  IsolatedWebAppUrlInfo iwa_1_url_info = InstallIsolatedWebApp(iwa_1_url);

  auto expected_iwa_1_sp_base = content::StoragePartitionConfig::Create(
      profile(), expected_partition_domain_1,
      /*partition_name=*/"", /*in_memory=*/false);

  content::StoragePartitionConfig output_iwa_1_sp_1 =
      RunCommand(iwa_1_url_info, "partition_name", /*in_memory=*/true);
  auto expected_iwa_1_sp_1 = content::StoragePartitionConfig::Create(
      profile(), expected_partition_domain_1,
      /*partition_name=*/"partition_name", /*in_memory=*/true);
  ASSERT_EQ(expected_iwa_1_sp_1, output_iwa_1_sp_1);

  content::StoragePartitionConfig output_iwa_1_sp_2 =
      RunCommand(iwa_1_url_info, "partition_name", /*in_memory=*/false);
  auto expected_iwa_1_sp_2 = content::StoragePartitionConfig::Create(
      profile(), expected_partition_domain_1,
      /*partition_name=*/"partition_name", /*in_memory=*/false);
  ASSERT_EQ(expected_iwa_1_sp_2, output_iwa_1_sp_2);

  // Set up IWA 2.
  const GURL iwa_2_url(
      "isolated-app://"
      "4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic");
  const std::string expected_partition_domain_2 =
      "ixhWrMZlUCk1eUZYDqDyJy4DZzylqxRZWbMlA4WqsfTo=";
  IsolatedWebAppUrlInfo iwa_2_url_info = InstallIsolatedWebApp(iwa_2_url);

  auto expected_iwa_2_sp_base = content::StoragePartitionConfig::Create(
      profile(), expected_partition_domain_2,
      /*partition_name=*/"", /*in_memory=*/false);

  content::StoragePartitionConfig output_iwa_2_sp_1 =
      RunCommand(iwa_2_url_info, "partition_name", /*in_memory=*/true);
  auto expected_iwa_2_sp_1 = content::StoragePartitionConfig::Create(
      profile(), expected_partition_domain_2,
      /*partition_name=*/"partition_name", /*in_memory=*/true);
  ASSERT_EQ(expected_iwa_2_sp_1, output_iwa_2_sp_1);

  content::StoragePartitionConfig output_iwa_2_sp_2 =
      RunCommand(iwa_2_url_info, "partition_name", /*in_memory=*/false);
  auto expected_iwa_2_sp_2 = content::StoragePartitionConfig::Create(
      profile(), expected_partition_domain_2,
      /*partition_name=*/"partition_name", /*in_memory=*/false);
  ASSERT_EQ(expected_iwa_2_sp_2, output_iwa_2_sp_2);

  // Check partitions
  std::vector<content::StoragePartitionConfig> iwa_1_sps =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(
          iwa_1_url_info.app_id());
  ASSERT_EQ(3UL, iwa_1_sps.size());
  EXPECT_THAT(iwa_1_sps, testing::UnorderedElementsAre(expected_iwa_1_sp_base,
                                                       expected_iwa_1_sp_1,
                                                       expected_iwa_1_sp_2));

  std::vector<content::StoragePartitionConfig> iwa_2_sps =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(
          iwa_2_url_info.app_id());
  ASSERT_EQ(3UL, iwa_2_sps.size());
  EXPECT_THAT(iwa_2_sps, testing::UnorderedElementsAre(expected_iwa_2_sp_base,
                                                       expected_iwa_2_sp_1,
                                                       expected_iwa_2_sp_2));
}

}  // namespace web_app
