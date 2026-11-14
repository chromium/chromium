// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/controlled_frame_partitions_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_command_scheduler.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class ControlledFramePartitionsManagerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  ControlledFramePartitionsManager& partitions_manager() {
    return ControlledFramePartitionsManager::From(fake_provider());
  }

  IsolatedWebAppUrlInfo InstallIsolatedWebApp() {
    std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
        IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
    return bundle->InstallChecked(profile());
  }

  content::StoragePartitionConfig RegisterPersistedControlledFramePartition(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name) {
    base::test::TestFuture<std::optional<content::StoragePartitionConfig>>
        future;
    IwaCommandScheduler::From(fake_provider())
        .GetControlledFramePartition(url_info, partition_name,
                                     /*in_memory=*/false, future.GetCallback());
    return future.Get().value();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebApps};
};

TEST_F(ControlledFramePartitionsManagerTest,
       SaveGetAndClearInMemoryPartitions) {
  IsolatedWebAppUrlInfo url_info_1 = InstallIsolatedWebApp();
  IsolatedWebAppUrlInfo url_info_2 = InstallIsolatedWebApp();

  EXPECT_THAT(partitions_manager().GetInMemoryPartitionConfigs(url_info_1),
              IsEmpty());

  auto output_config_1 = partitions_manager().SaveAndGetInMemoryPartitionConfig(
      url_info_1, "partition_1");
  auto output_config_2 = partitions_manager().SaveAndGetInMemoryPartitionConfig(
      url_info_1, "partition_2");
  auto other_app_config =
      partitions_manager().SaveAndGetInMemoryPartitionConfig(url_info_2,
                                                             "other_partition");

  auto expected_config_cf_1 =
      url_info_1.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_1", /*in_memory=*/true);
  auto expected_config_cf_2 =
      url_info_1.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_2", /*in_memory=*/true);
  auto expected_other_app_config =
      url_info_2.GetStoragePartitionConfigForControlledFrame(
          profile(), "other_partition", /*in_memory=*/true);

  EXPECT_EQ(expected_config_cf_1, output_config_1);
  EXPECT_EQ(expected_config_cf_2, output_config_2);
  EXPECT_EQ(expected_other_app_config, other_app_config);
  EXPECT_THAT(partitions_manager().GetInMemoryPartitionConfigs(url_info_1),
              UnorderedElementsAre(expected_config_cf_1, expected_config_cf_2));

  partitions_manager().ClearInMemoryPartitions(url_info_1.app_id());
  EXPECT_THAT(partitions_manager().GetInMemoryPartitionConfigs(url_info_1),
              IsEmpty());
  EXPECT_THAT(partitions_manager().GetInMemoryPartitionConfigs(url_info_2),
              UnorderedElementsAre(expected_other_app_config));
}

TEST_F(ControlledFramePartitionsManagerTest, AsDebugValue) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

  EXPECT_TRUE(partitions_manager()
                  .AsDebugValue()
                  .GetDict()
                  .FindDict("in_memory_controlled_frame_partitions")
                  ->empty());

  ASSERT_TRUE(partitions_manager()
                  .SaveAndGetInMemoryPartitionConfig(url_info, "partition_1")
                  .has_value());
  ASSERT_TRUE(partitions_manager()
                  .SaveAndGetInMemoryPartitionConfig(url_info, "partition_2")
                  .has_value());

  base::Value debug_value = partitions_manager().AsDebugValue();
  const base::ListValue* debug_partitions =
      debug_value.GetDict()
          .FindDict("in_memory_controlled_frame_partitions")
          ->FindList(url_info.app_id());
  ASSERT_TRUE(debug_partitions);
  EXPECT_EQ(*debug_partitions,
            base::ListValue().Append("partition_1").Append("partition_2"));

  partitions_manager().ClearInMemoryPartitions(url_info.app_id());
  EXPECT_EQ(partitions_manager()
                .AsDebugValue()
                .GetDict()
                .FindDict("in_memory_controlled_frame_partitions")
                ->FindList(url_info.app_id()),
            nullptr);
}

TEST_F(ControlledFramePartitionsManagerTest, RejectsMissingOrNonIsolatedApp) {
  test::InstallDummyWebApp(profile(), "Non-IWA", GURL("https://example.com"));

  IsolatedWebAppUrlInfo uninstalled_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateRandomForProxyMode());

  EXPECT_EQ(partitions_manager().SaveAndGetInMemoryPartitionConfig(
                uninstalled_url_info, "partition_1"),
            std::nullopt);
  EXPECT_THAT(
      partitions_manager().GetInMemoryPartitionConfigs(uninstalled_url_info),
      IsEmpty());
}

TEST_F(ControlledFramePartitionsManagerTest,
       UninstallClearsInMemoryPartitionsAfterBrowsingDataRemoval) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());

  std::optional<content::StoragePartitionConfig> in_memory_cf_config =
      partitions_manager().SaveAndGetInMemoryPartitionConfig(url_info,
                                                             "ephemeral_cf");
  ASSERT_TRUE(in_memory_cf_config.has_value());

  std::vector<std::optional<content::StoragePartitionConfig>> cleared_configs;
  base::RunLoop remover_loop;
  content::BrowsingDataRemover* remover = profile()->GetBrowsingDataRemover();
  remover->SetWouldCompleteCallbackForTesting(
      base::BindLambdaForTesting([&](base::OnceClosure continue_callback) {
        cleared_configs.push_back(
            remover->GetLastUsedStoragePartitionConfigForTesting());
        if (remover->GetPendingTaskCountForTesting() == 1) {
          remover_loop.Quit();
        }
        std::move(continue_callback).Run();
      }));

  base::test::TestFuture<webapps::UninstallResultCode> uninstall_future;
  fake_provider().scheduler().RemoveUserUninstallableManagements(
      url_info.app_id(), webapps::WebappUninstallSource::kAppsPage,
      uninstall_future.GetCallback());
  EXPECT_EQ(uninstall_future.Get(), webapps::UninstallResultCode::kAppRemoved);
  remover_loop.Run();
  remover->SetWouldCompleteCallbackForTesting(base::NullCallback());

  // The in-memory Controlled Frame partition must still be visible while
  // BrowsingDataRemover runs during uninstall, and erased once removal
  // completes so a subsequent reinstall starts clean.
  EXPECT_THAT(cleared_configs, Contains(*in_memory_cf_config));
  EXPECT_THAT(partitions_manager().GetInMemoryPartitionConfigs(url_info),
              IsEmpty());

  IsolatedWebAppUrlInfo reinstalled_info = bundle->InstallChecked(profile());
  EXPECT_EQ(reinstalled_info.app_id(), url_info.app_id());
  EXPECT_THAT(
      partitions_manager().GetInMemoryPartitionConfigs(reinstalled_info),
      IsEmpty());
}

TEST_F(ControlledFramePartitionsManagerTest,
       GetStoragePartitionConfigsReturnsBasePersistentAndInMemoryPartitions) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

  content::StoragePartitionConfig persisted_1 =
      RegisterPersistedControlledFramePartition(url_info, "persisted_1");
  content::StoragePartitionConfig persisted_2 =
      RegisterPersistedControlledFramePartition(url_info, "persisted_2");
  std::optional<content::StoragePartitionConfig> in_memory_1 =
      partitions_manager().SaveAndGetInMemoryPartitionConfig(url_info,
                                                             "in_memory_1");
  ASSERT_TRUE(in_memory_1.has_value());

  EXPECT_THAT(GetIsolatedWebAppStoragePartitionConfigs(fake_provider(),
                                                       url_info.app_id()),
              UnorderedElementsAre(url_info.storage_partition_config(profile()),
                                   persisted_1, persisted_2, *in_memory_1));
}

TEST_F(ControlledFramePartitionsManagerTest,
       GetStoragePartitionConfigsReturnsEmptyForMissingOrNonIsolatedApp) {
  EXPECT_THAT(GetIsolatedWebAppStoragePartitionConfigs(fake_provider(),
                                                       "missing_app_id"),
              IsEmpty());

  const webapps::AppId non_iwa_id = test::InstallDummyWebApp(
      profile(), "Non-IWA", GURL("https://example.com"));
  EXPECT_THAT(
      GetIsolatedWebAppStoragePartitionConfigs(fake_provider(), non_iwa_id),
      IsEmpty());
}

TEST_F(ControlledFramePartitionsManagerTest,
       GetStoragePartitionConfigsReturnsEmptyWhenIsolatedWebAppsDisabled) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(features::kIsolatedWebApps);
  EXPECT_THAT(GetIsolatedWebAppStoragePartitionConfigs(fake_provider(),
                                                       url_info.app_id()),
              IsEmpty());
}

TEST_F(ControlledFramePartitionsManagerTest,
       GetStoragePartitionConfigsReturnsPartitionsWhileUninstalling) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  content::StoragePartitionConfig persisted_1 =
      RegisterPersistedControlledFramePartition(url_info, "persisted_1");
  std::optional<content::StoragePartitionConfig> in_memory_1 =
      partitions_manager().SaveAndGetInMemoryPartitionConfig(url_info,
                                                             "in_memory_1");
  ASSERT_TRUE(in_memory_1.has_value());

  bool observed_uninstalling_state = false;
  bool matches_isolated_app_during_uninstall = true;
  std::optional<content::StoragePartitionConfig> save_during_uninstall;
  std::vector<content::StoragePartitionConfig> partitions_during_uninstall;

  base::RunLoop remover_loop;
  content::BrowsingDataRemover* remover = profile()->GetBrowsingDataRemover();
  remover->SetWouldCompleteCallbackForTesting(
      base::BindLambdaForTesting([&](base::OnceClosure continue_callback) {
        if (!observed_uninstalling_state) {
          observed_uninstalling_state = true;
          matches_isolated_app_during_uninstall =
              fake_provider().registrar_unsafe().AppMatches(
                  url_info.app_id(), WebAppFilter::IsIsolatedApp());
          save_during_uninstall =
              partitions_manager().SaveAndGetInMemoryPartitionConfig(
                  url_info, "rejected_during_uninstall");
          partitions_during_uninstall =
              GetIsolatedWebAppStoragePartitionConfigs(fake_provider(),
                                                       url_info.app_id());
        }
        if (remover->GetPendingTaskCountForTesting() == 1) {
          remover_loop.Quit();
        }
        std::move(continue_callback).Run();
      }));

  base::test::TestFuture<webapps::UninstallResultCode> uninstall_future;
  fake_provider().scheduler().RemoveUserUninstallableManagements(
      url_info.app_id(), webapps::WebappUninstallSource::kAppsPage,
      uninstall_future.GetCallback());
  EXPECT_EQ(uninstall_future.Get(), webapps::UninstallResultCode::kAppRemoved);
  remover_loop.Run();
  remover->SetWouldCompleteCallbackForTesting(base::NullCallback());

  EXPECT_TRUE(observed_uninstalling_state);
  EXPECT_FALSE(matches_isolated_app_during_uninstall);
  EXPECT_EQ(save_during_uninstall, std::nullopt);
  EXPECT_THAT(partitions_during_uninstall,
              UnorderedElementsAre(url_info.storage_partition_config(profile()),
                                   persisted_1, *in_memory_1));
  EXPECT_THAT(GetIsolatedWebAppStoragePartitionConfigs(fake_provider(),
                                                       url_info.app_id()),
              IsEmpty());
}

}  // namespace
}  // namespace web_app
