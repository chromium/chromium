// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_migrate_to_app_command.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scheduler/install_migrate_to_app_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

class InstallMigrateToAppCommandTest : public WebAppTest {
 public:
  const GURL kSourceUrl = GURL("https://source.com");
  const webapps::ManifestId kSourceManifestId =
      webapps::ManifestId(GURL("https://source.com/id"));
  const GURL kTargetUrl = GURL("https://target.com");
  const webapps::ManifestId kTargetManifestId =
      webapps::ManifestId(GURL("https://target.com/id"));
  const GURL kTargetInstallUrl = GURL("https://target.com/install");

  InstallMigrateToAppCommandTest() = default;

  void SetUp() override {
    WebAppTest::SetUp();
    fake_provider().SetWebContentsManager(
        std::make_unique<FakeWebContentsManager>());
    auto origin_association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    origin_association_manager->set_pass_through(true);
    fake_provider().SetOriginAssociationManager(
        std::move(origin_association_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  webapps::AppId InstallSourceApp() {
    auto web_app_install_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kSourceUrl);
    web_app_install_info->title = u"Source App";
    web_app_install_info->SetManifestIdAndStartUrl(kSourceManifestId,
                                                   kSourceUrl);
    return test::InstallWebApp(profile(), std::move(web_app_install_info));
  }

  void SetupTargetPageState(const GURL& manifest_id,
                            const GURL& migrate_from_id) {
    auto& page_state =
        web_contents_manager().GetOrCreatePageState(kTargetInstallUrl);
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;

    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kTargetUrl;
    manifest->id = manifest_id;
    manifest->name = u"Target App";

    auto migrate_from = blink::mojom::ManifestMigrateFrom::New();
    migrate_from->id = migrate_from_id;
    manifest->migrate_from.push_back(std::move(migrate_from));

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  InstallMigrateToAppResult ScheduleCommandAndWait(
      const webapps::ManifestId& source_manifest_id,
      const webapps::ManifestId& target_manifest_id,
      const GURL& target_install_url) {
    base::test::TestFuture<InstallMigrateToAppResult> future;
    fake_provider().scheduler().ScheduleInstallMigrateToApp(
        source_manifest_id, target_manifest_id, target_install_url,
        future.GetCallback());
    return future.Get();
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(InstallMigrateToAppCommandTest, SuccessInstall) {
  webapps::AppId source_app_id = InstallSourceApp();
  SetupTargetPageState(kTargetManifestId, kSourceManifestId);

  EXPECT_EQ(ScheduleCommandAndWait(kSourceManifestId, kTargetManifestId,
                                   kTargetInstallUrl),
            InstallMigrateToAppResult::kSuccessNewInstall);

  webapps::AppId target_app_id = GenerateAppIdFromManifestId(kTargetManifestId);
  EXPECT_TRUE(fake_provider()
                  .registrar_unsafe()
                  .GetInstallState(target_app_id)
                  .has_value());
  EXPECT_EQ(fake_provider().registrar_unsafe().GetInstallState(target_app_id),
            proto::InstallState::SUGGESTED_FROM_MIGRATION);
  histogram_tester_.ExpectUniqueSample(
      "WebApp.InstallMigrateToApp.Result",
      InstallMigrateToAppResult::kSuccessNewInstall, 1);
}

TEST_F(InstallMigrateToAppCommandTest, SuccessUpdate) {
  webapps::AppId source_app_id = InstallSourceApp();
  SetupTargetPageState(kTargetManifestId, kSourceManifestId);

  // Pre-install the target app.
  auto web_app_install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kTargetUrl);
  web_app_install_info->title = u"Target App";
  web_app_install_info->SetManifestIdAndStartUrl(kTargetManifestId, kTargetUrl);
  webapps::AppId target_app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info));

  EXPECT_EQ(ScheduleCommandAndWait(kSourceManifestId, kTargetManifestId,
                                   kTargetInstallUrl),
            InstallMigrateToAppResult::kSuccessAlreadyInstalled);

  EXPECT_TRUE(fake_provider()
                  .registrar_unsafe()
                  .GetInstallState(target_app_id)
                  .has_value());
  histogram_tester_.ExpectUniqueSample(
      "WebApp.InstallMigrateToApp.Result",
      InstallMigrateToAppResult::kSuccessAlreadyInstalled, 1);
}

TEST_F(InstallMigrateToAppCommandTest, SuccessUpdateSuggested) {
  webapps::AppId source_app_id = InstallSourceApp();
  webapps::AppId target_app_id = GenerateAppIdFromManifestId(kTargetManifestId);
  SetupTargetPageState(kTargetManifestId, kSourceManifestId);

  // Pre-install the target app as suggested-from-another-device.
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_web_app()->set_name("Target App");
  entity_data.specifics.mutable_web_app()->set_start_url(kTargetUrl.spec());
  entity_data.specifics.mutable_web_app()->set_relative_manifest_id(
      RelativeManifestIdPath(kTargetManifestId));

  WebAppSyncBridge& sync_bridge = fake_provider().sync_bridge_unsafe();

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(syncer::EntityChange::CreateAdd(
      GenerateAppIdFromManifestId(kTargetManifestId), std::move(entity_data)));
  sync_bridge.ApplyIncrementalSyncChanges(
      sync_bridge.CreateMetadataChangeList(), std::move(entity_changes));
  fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();

  proto::InstallState expected_install_state =
      AreAppsLocallyInstalledBySync()
          ? proto::InstallState::INSTALLED_WITH_OS_INTEGRATION
          : proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
  auto install_state =
      fake_provider().registrar_unsafe().GetInstallState(target_app_id);
  ASSERT_TRUE(install_state.has_value());
  EXPECT_EQ(install_state, expected_install_state);

  // Update target page state with a new name to verify identity update.
  auto& page_state =
      web_contents_manager().GetOrCreatePageState(kTargetInstallUrl);
  page_state.manifest_before_default_processing->name = u"New Target Name";

  EXPECT_EQ(ScheduleCommandAndWait(kSourceManifestId, kTargetManifestId,
                                   kTargetInstallUrl),
            InstallMigrateToAppResult::kSuccessAlreadyInstalled);

  EXPECT_EQ(fake_provider()
                .registrar_unsafe()
                .GetAppById(target_app_id)
                ->untranslated_name(),
            "Target App");
  histogram_tester_.ExpectUniqueSample(
      "WebApp.InstallMigrateToApp.Result",
      InstallMigrateToAppResult::kSuccessAlreadyInstalled, 1);
}

TEST_F(InstallMigrateToAppCommandTest, UrlLoadFailure) {
  webapps::AppId source_app_id = InstallSourceApp();
  auto& page_state =
      web_contents_manager().GetOrCreatePageState(kTargetInstallUrl);
  page_state.url_load_result =
      webapps::WebAppUrlLoaderResult::kFailedUnknownReason;

  EXPECT_EQ(ScheduleCommandAndWait(kSourceManifestId, kTargetManifestId,
                                   kTargetInstallUrl),
            InstallMigrateToAppResult::kUrlLoadFailed);
  histogram_tester_.ExpectUniqueSample(
      "WebApp.InstallMigrateToApp.Result",
      InstallMigrateToAppResult::kUrlLoadFailed, 1);
}

TEST_F(InstallMigrateToAppCommandTest, ManifestIdMismatch) {
  webapps::AppId source_app_id = InstallSourceApp();
  SetupTargetPageState(GURL("https://wrong-id.com"), kSourceManifestId);

  EXPECT_EQ(ScheduleCommandAndWait(kSourceManifestId, kTargetManifestId,
                                   kTargetInstallUrl),
            InstallMigrateToAppResult::kManifestIdMismatch);
  histogram_tester_.ExpectUniqueSample(
      "WebApp.InstallMigrateToApp.Result",
      InstallMigrateToAppResult::kManifestIdMismatch, 1);
}

TEST_F(InstallMigrateToAppCommandTest, MigrateFromMismatch) {
  webapps::AppId source_app_id = InstallSourceApp();
  SetupTargetPageState(kTargetManifestId, GURL("https://wrong-source.com"));

  EXPECT_EQ(ScheduleCommandAndWait(kSourceManifestId, kTargetManifestId,
                                   kTargetInstallUrl),
            InstallMigrateToAppResult::kMigrateFromMismatch);
  histogram_tester_.ExpectUniqueSample(
      "WebApp.InstallMigrateToApp.Result",
      InstallMigrateToAppResult::kMigrateFromMismatch, 1);
}

}  // namespace web_app
