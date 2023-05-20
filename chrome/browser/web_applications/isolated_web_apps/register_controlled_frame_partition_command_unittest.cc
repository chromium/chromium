// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/register_controlled_frame_partition_command.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
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

class RegisterControlledFramePartitionCommandTest : public WebAppTest {
 public:
  RegisterControlledFramePartitionCommandTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

  void SetUp() override {
    WebAppTest::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void RunCommand(const IsolatedWebAppUrlInfo& url_info,
                  const std::string& partition_name) {
    base::RunLoop loop;
    provider().scheduler().ScheduleCallbackWithLock(
        "RegisterControlledFramePartition",
        std::make_unique<AppLockDescription>(url_info.app_id()),
        base::BindOnce(&RegisterControlledFramePartitionWithLock,
                       url_info.app_id(), partition_name, loop.QuitClosure()),
        FROM_HERE);
    loop.Run();
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

TEST_F(RegisterControlledFramePartitionCommandTest, CanRegisterPartition) {
  const GURL app_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(app_url);

  RunCommand(url_info, "partition name");

  std::vector<content::StoragePartitionConfig> storage_partitions =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(url_info.app_id());
  ASSERT_THAT(storage_partitions,
              UnorderedElementsAre(
                  url_info.storage_partition_config(profile()),
                  url_info.GetStoragePartitionConfigForControlledFrame(
                      profile(), "partition name", /*in_memory=*/false)));
}

TEST_F(RegisterControlledFramePartitionCommandTest,
       CanRegisterMultiplePartitions) {
  const GURL app_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(app_url);

  RunCommand(url_info, "partition name 1");
  RunCommand(url_info, "partition name 2");

  std::vector<content::StoragePartitionConfig> storage_partitions =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(url_info.app_id());
  ASSERT_THAT(storage_partitions,
              UnorderedElementsAre(
                  url_info.storage_partition_config(profile()),
                  url_info.GetStoragePartitionConfigForControlledFrame(
                      profile(), "partition name 1", /*in_memory=*/false),
                  url_info.GetStoragePartitionConfigForControlledFrame(
                      profile(), "partition name 2", /*in_memory=*/false)));
}

TEST_F(RegisterControlledFramePartitionCommandTest,
       DuplicatePartitionsIgnored) {
  const GURL app_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(app_url);

  RunCommand(url_info, "partition name");
  RunCommand(url_info, "partition name");

  std::vector<content::StoragePartitionConfig> storage_partitions =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(url_info.app_id());
  ASSERT_THAT(storage_partitions,
              UnorderedElementsAre(
                  url_info.storage_partition_config(profile()),
                  url_info.GetStoragePartitionConfigForControlledFrame(
                      profile(), "partition name", /*in_memory=*/false)));
}

}  // namespace web_app
