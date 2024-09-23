// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/garbage_collect_storage_partitions_command.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/get_controlled_frame_partition_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

constexpr std::string_view kIwa1UrlString(
    "isolated-app://wiabxazz27gf4rgupuiogazvf3u4pszqgzp2tlocmvnhpkyzsvnaaaac/");
constexpr std::string_view kIwa2UrlString(
    "isolated-app://lcqmu3b7fzkmcev36j2slaxolx6edzzinw7n4xhppqsk4wo3hkfaaaac/");

const base::FilePath::CharType kStoragePartitionDirname[] =
    FILE_PATH_LITERAL("Storage");
const base::FilePath::CharType kExtensionsDirname[] = FILE_PATH_LITERAL("ext");

namespace {

bool PathHasSubPath(base::FilePath absolute_parent,
                    base::FilePath relative_children) {
  base::FileEnumerator enumerator(
      absolute_parent, /*recursive=*/false,
      /*file_type=*/base::FileEnumerator::FileType::DIRECTORIES);
  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    if (path.BaseName() == relative_children) {
      return true;
    }
  }
  return false;
}

std::u16string SubDirDebugValue(base::FilePath absolute_parent) {
  std::u16string debug_output;
  debug_output += u"The sub directories of " +
                  absolute_parent.LossyDisplayName() + u" are:\n";

  base::FileEnumerator enumerator(
      absolute_parent, /*recursive=*/false,
      /*file_type=*/base::FileEnumerator::FileType::DIRECTORIES);
  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    debug_output += (path.LossyDisplayName()) + u"\n";
  }
  return debug_output;
}

}  // namespace

namespace web_app {

class GarbageCollectStoragePartitionsCommandBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  // Returns an absolute path to the Storage Partition Dir.
  // The constants are from `content/browser/storage_partition_impl_map.cc`.
  base::FilePath storage_partition_path() {
    return profile()
        ->GetPath()
        .Append(kStoragePartitionDirname)
        .Append(kExtensionsDirname);
  }

  base::FilePath UrlToRelativeDomainPath(const GURL& url) {
    auto url_info = IsolatedWebAppUrlInfo::Create(url);
    content::StoragePartitionConfig sp_config =
        url_info->storage_partition_config(profile());
    return base::FilePath::FromUTF8Unsafe(sp_config.partition_domain());
  }

  IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebAppWithScope(
      const url::Origin& proxy_origin,
      const GURL& scope_url) {
    base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>>
        future;

    auto url_info = IsolatedWebAppUrlInfo::Create(scope_url);
    CHECK(url_info.has_value());

    WebAppProvider::GetForWebApps(profile())->scheduler().InstallIsolatedWebApp(
        url_info.value(),
        IsolatedWebAppInstallSource::FromDevUi(IwaSourceProxy(proxy_origin)),
        /*expected_version=*/std::nullopt,
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());

    CHECK(future.Get().has_value()) << future.Get().error();
    return url_info.value();
  }

  content::StoragePartitionConfig AddPersistentStoragePartitonToIwa(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name,
      const base::Location location = FROM_HERE) {
    base::test::TestFuture<std::optional<content::StoragePartitionConfig>>
        future;
    provider().scheduler().ScheduleCallbackWithResult(
        "GetControlledFramePartition", AppLockDescription(url_info.app_id()),
        base::BindOnce(&GetControlledFramePartitionWithLock, profile(),
                       url_info, partition_name, /*in_memory=*/false),
        future.GetCallback(), /*arg_for_shutdown=*/
        std::optional<content::StoragePartitionConfig>(std::nullopt), location);
    return future.Get().value();
  }
};

// This test does the following
// - Install 2 IWAs.
// - Ensure all Storage Partitions exist.
// - Uninstall an IWA.
// - Run Garbage Collection.
// - Ensure uninstalled IWA Storage Partitions are cleaned up.
// - Ensure installed IWA Storage Partitions are not cleaned up.
IN_PROC_BROWSER_TEST_F(GarbageCollectStoragePartitionsCommandBrowserTest,
                       PRE_UninstalledAppsHaveStoragePartitionsCleanedUp) {
  base::ScopedAllowBlockingForTesting blocking_allow;
  const GURL kIwa1Url(kIwa1UrlString);
  const GURL kIwa2Url(kIwa2UrlString);

  // Install 2 IWAs.
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  IsolatedWebAppUrlInfo url_info_1 = InstallDevModeProxyIsolatedWebAppWithScope(
      isolated_web_app_dev_server->GetOrigin(), kIwa1Url);
  content::StoragePartitionConfig iwa_1_base_sp_config =
      url_info_1.storage_partition_config(profile());
  ASSERT_TRUE(profile()->GetStoragePartition(iwa_1_base_sp_config,
                                             /*can_create=*/false));
  content::StoragePartitionConfig iwa_1_sp_1_config =
      AddPersistentStoragePartitonToIwa(url_info_1, "partition");
  // The previous command only registers the partition, therefore we need to
  // create it here.
  ASSERT_TRUE(
      profile()->GetStoragePartition(iwa_1_sp_1_config, /*can_create=*/true));

  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_2 =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  IsolatedWebAppUrlInfo url_info_2 = InstallDevModeProxyIsolatedWebAppWithScope(
      isolated_web_app_dev_server->GetOrigin(), kIwa2Url);
  content::StoragePartitionConfig iwa_2_base_sp_config =
      url_info_2.storage_partition_config(profile());
  ASSERT_TRUE(profile()->GetStoragePartition(iwa_2_base_sp_config,
                                             /*can_create=*/false));
  content::StoragePartitionConfig iwa_2_sp_1_config =
      AddPersistentStoragePartitonToIwa(url_info_2, "partition");
  // The previous command only registers the partition, therefore we need to
  // create it here.
  ASSERT_TRUE(
      profile()->GetStoragePartition(iwa_2_sp_1_config, /*can_create=*/true));

  // Uninstall one of the IWAs.
  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider().scheduler().RemoveUserUninstallableManagements(
      url_info_2.app_id(), webapps::WebappUninstallSource::kAppsPage,
      future.GetCallback());
  auto code = future.Get();
  ASSERT_TRUE(code == webapps::UninstallResultCode::kAppRemoved);

  // Pref value is set.
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kShouldGarbageCollectStoragePartitions));
  // Both paths exist before garbage collection.
  ASSERT_TRUE(PathHasSubPath(storage_partition_path(),
                             UrlToRelativeDomainPath(kIwa1Url)))
      << SubDirDebugValue(storage_partition_path());
  ASSERT_TRUE(PathHasSubPath(storage_partition_path(),
                             UrlToRelativeDomainPath(kIwa2Url)))
      << SubDirDebugValue(storage_partition_path());
}

IN_PROC_BROWSER_TEST_F(GarbageCollectStoragePartitionsCommandBrowserTest,
                       UninstalledAppsHaveStoragePartitionsCleanedUp) {
  base::ScopedAllowBlockingForTesting blocking_allow;

  base::RunLoop run_loop;
  provider()
      .isolated_web_app_installation_manager()
      .on_garbage_collect_storage_partitions_done_for_testing()
      .Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  const GURL kIwa1Url(kIwa1UrlString);
  const GURL kIwa2Url(kIwa2UrlString);

  ASSERT_TRUE(base::PathExists(storage_partition_path()));
  // Only IWA1's path still exists.
  EXPECT_TRUE(PathHasSubPath(storage_partition_path(),
                             UrlToRelativeDomainPath(kIwa1Url)))
      << SubDirDebugValue(storage_partition_path());
  EXPECT_FALSE(PathHasSubPath(storage_partition_path(),
                              UrlToRelativeDomainPath(kIwa2Url)))
      << SubDirDebugValue(storage_partition_path());
}

}  // namespace web_app
