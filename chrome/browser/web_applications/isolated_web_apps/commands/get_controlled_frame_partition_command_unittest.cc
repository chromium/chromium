// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/get_controlled_frame_partition_command.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "url/gurl.h"

using ::testing::UnorderedElementsAre;

namespace web_app {

class GetControlledFramePartitionCommandTest : public WebAppTest {
 public:
  GetControlledFramePartitionCommandTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
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

  IsolatedWebAppUrlInfo InstallIsolatedWebApp() {
    const std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle =
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
            .BuildBundle();
    bundle->TrustSigningKey();
    bundle->FakeInstallPageState(profile());
    return bundle->InstallChecked(profile());
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // isolated web app builder uses json parser from the decoder
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(GetControlledFramePartitionCommandTest, CanRegisterPartition) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

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
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

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
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
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
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

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
  IsolatedWebAppUrlInfo iwa_1_url_info = InstallIsolatedWebApp();
  const std::string expected_partition_domain_1 =
      iwa_1_url_info.storage_partition_config(profile()).partition_domain();

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
  IsolatedWebAppUrlInfo iwa_2_url_info = InstallIsolatedWebApp();
  const std::string expected_partition_domain_2 =
      iwa_2_url_info.storage_partition_config(profile()).partition_domain();

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
