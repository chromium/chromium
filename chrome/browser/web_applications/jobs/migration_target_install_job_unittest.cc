// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/migration_target_install_job.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/default_clock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest_migration_behavior.mojom.h"

namespace web_app {

class MigrationTargetInstallJobTest : public WebAppTest {
 public:
  MigrationTargetInstallJobTest()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory()) {}

  void SetUp() override {
    WebAppTest::SetUp();
    fake_provider().SetWebContentsManager(
        std::make_unique<FakeWebContentsManager>());
    auto origin_association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>(*profile());
    origin_association_manager->set_pass_through(true);
    fake_provider().SetOriginAssociationManager(
        std::move(origin_association_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  MigrationTargetInstallJobResult RunJob(blink::Manifest manifest) {
    webapps::AppId app_id = GenerateAppIdFromManifest(manifest);
    base::test::TestFuture<void> lock_future;
    auto lock = std::make_unique<AppLock>();
    fake_provider().command_manager().lock_manager().AcquireLock(
        AppLockDescription(app_id), *lock, lock_future.GetCallback(),
        FROM_HERE);
    EXPECT_TRUE(lock_future.Wait());

    base::test::TestFuture<MigrationTargetInstallJobResult> future;
    base::DictValue debug_value;
    auto data_retriever = web_contents_manager().CreateDataRetriever();
    auto job = MigrationTargetInstallJob::CreateAndStart(
        std::move(manifest), web_contents()->GetWeakPtr(), profile(),
        data_retriever.get(), &debug_value, lock.get(), lock.get(),
        future.GetCallback());
    return future.Get();
  }
};

TEST_F(MigrationTargetInstallJobTest, InstallNewApp) {
  blink::Manifest::MigrateFrom migrate_from;
  migrate_from.id = GURL("https://example.com/old_app");
  migrate_from.behavior = blink::mojom::ManifestMigrationBehavior::kSuggest;

  blink::Manifest::Builder builder(GURL("https://example.com/start"),
                                   GURL("https://example.com/start"),
                                   GURL("https://example.com/"));
  builder.SetName(u"New App");
  builder.SetManifestUrl(GURL("https://example.com/start"));
  builder.SetMigrateFrom({std::move(migrate_from)});
  blink::Manifest manifest = std::move(builder).BuildChecked();

  webapps::AppId app_id = GenerateAppIdFromManifest(manifest);

  EXPECT_EQ(RunJob(std::move(manifest)),
            MigrationTargetInstallJobResult::kSuccessInstalled);

  EXPECT_TRUE(fake_provider().registrar_unsafe().GetAppById(app_id));
  EXPECT_EQ(fake_provider().registrar_unsafe().GetInstallState(app_id),
            proto::InstallState::SUGGESTED_FROM_MIGRATION);
  EXPECT_TRUE(fake_provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::IsAppSuggestedForMigration()));
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::IsAppSurfaceableToUser()));
}

TEST_F(MigrationTargetInstallJobTest, UpdateInstalledApp) {
  blink::Manifest::ImageResource icon_info;
  icon_info.src = GURL("https://example.com/icon.png");
  icon_info.sizes.push_back(gfx::Size(192, 192));
  icon_info.purpose.push_back(
      blink::mojom::ManifestImageResource::Purpose::ANY);

  blink::Manifest::MigrateFrom migrate_from;
  migrate_from.id = GURL("https://example.com/old_app");
  migrate_from.behavior = blink::mojom::ManifestMigrationBehavior::kSuggest;

  // The name in the manifest is "New App Name", which is a security-sensitive
  // update.
  blink::Manifest::Builder update_builder(GURL("https://example.com/start"),
                                          GURL("https://example.com/start"),
                                          GURL("https://example.com/"));
  update_builder.SetName(u"New App Name");
  update_builder.SetManifestUrl(GURL("https://example.com/start"));
  update_builder.SetIcons({std::move(icon_info)});
  update_builder.SetMigrateFrom({std::move(migrate_from)});
  blink::Manifest manifest = std::move(update_builder).BuildChecked();

  webapps::AppId app_id = GenerateAppIdFromManifest(manifest);
  test::InstallDummyWebApp(profile(), "Old App Name",
                           GURL("https://example.com/start"));

  {
    FakeWebContentsManager::FakeIconState icon_state;
    icon_state.bitmaps.push_back(CreateSquareIcon(192, SK_ColorRED));
    web_contents_manager().SetIconState(GURL("https://example.com/icon.png"),
                                        std::move(icon_state));
  }

  EXPECT_EQ(RunJob(std::move(manifest)),
            MigrationTargetInstallJobResult::kAlreadyInstalled);

  // The app name should NOT have been updated.
  EXPECT_EQ(fake_provider()
                .registrar_unsafe()
                .GetAppById(app_id)
                ->untranslated_name(),
            "Old App Name");
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::IsAppSuggestedForMigration()));
  EXPECT_TRUE(fake_provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::IsAppSurfaceableToUser()));
}

TEST_F(MigrationTargetInstallJobTest, NoUpdateSuggestedApp) {
  blink::Manifest::ImageResource icon_info;
  icon_info.src = GURL("https://example.com/icon.png");
  icon_info.sizes.push_back(gfx::Size(192, 192));
  icon_info.purpose.push_back(
      blink::mojom::ManifestImageResource::Purpose::ANY);

  blink::Manifest::MigrateFrom migrate_from;
  migrate_from.id = GURL("https://example.com/old_app");
  migrate_from.behavior = blink::mojom::ManifestMigrationBehavior::kSuggest;

  // The name in the manifest is "New App Name", which is a security-sensitive
  // update.
  blink::Manifest::Builder no_update_builder(GURL("https://example.com/start"),
                                             GURL("https://example.com/start"),
                                             GURL("https://example.com/"));
  no_update_builder.SetName(u"New App Name");
  no_update_builder.SetManifestUrl(GURL("https://example.com/start"));
  no_update_builder.SetIcons({std::move(icon_info)});
  no_update_builder.SetMigrateFrom({std::move(migrate_from)});
  blink::Manifest manifest = std::move(no_update_builder).BuildChecked();

  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Old App Name", GURL("https://example.com/start"));
  {
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_id)->SetInstallState(
        proto::InstallState::SUGGESTED_FROM_MIGRATION);
  }

  {
    FakeWebContentsManager::FakeIconState icon_state;
    icon_state.bitmaps.push_back(CreateSquareIcon(192, SK_ColorRED));
    web_contents_manager().SetIconState(GURL("https://example.com/icon.png"),
                                        std::move(icon_state));
  }

  EXPECT_EQ(RunJob(std::move(manifest)),
            MigrationTargetInstallJobResult::kAlreadyInstalled);

  // The app name SHOULD NOT have been updated.
  EXPECT_EQ(fake_provider()
                .registrar_unsafe()
                .GetAppById(app_id)
                ->untranslated_name(),
            "Old App Name");
  EXPECT_TRUE(fake_provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::IsAppSuggestedForMigration()));
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::IsAppSurfaceableToUser()));
}

}  // namespace web_app
