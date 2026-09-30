// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/update_validated_origin_associations_command.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/model/pending_migration_info.h"
#include "chrome/browser/web_applications/scheduler/update_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using testing::_;

class MockWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;
  MOCK_METHOD(void,
              ScheduleResolveWebAppPendingMigrationInfo,
              (base::OnceClosure callback, const base::Location& location),
              (override));
};

class MockWebAppRegistrarObserver : public WebAppRegistrarObserver {
 public:
  MOCK_METHOD(void, OnAppRegistrarDestroyed, (), (override));
  MOCK_METHOD(void,
              OnWebAppEffectiveScopeChanged,
              (const webapps::AppId& app_id, const WebAppScope& new_scope),
              (override));
};

}  // namespace

class UpdateValidatedOriginAssociationsCommandTest : public WebAppTest {
 public:
  UpdateValidatedOriginAssociationsCommandTest() = default;
  ~UpdateValidatedOriginAssociationsCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    auto origin_association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>(*profile());
    fake_origin_association_manager_ = origin_association_manager.get();
    fake_provider().SetOriginAssociationManager(
        std::move(origin_association_manager));

    auto scheduler =
        std::make_unique<testing::NiceMock<MockWebAppCommandScheduler>>(
            *profile());
    mock_scheduler_ = scheduler.get();
    fake_provider().SetScheduler(std::move(scheduler));

    clock_ = std::make_unique<base::SimpleTestClock>();
    clock_->SetNow(base::Time::Now());

    fake_provider().SetClockForTesting(clock_.get());

    test::AwaitStartWebAppProviderAndSubsystems(profile());
    content::GetNetworkService();
  }

  void TearDown() override {
    fake_origin_association_manager_ = nullptr;
    mock_scheduler_ = nullptr;
    WebAppTest::TearDown();
  }

  webapps::AppId InstallApp(const GURL& start_url,
                            const ScopeExtensions& scope_extensions) {
    auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    info->title = u"Test App";
    info->scope_extensions = scope_extensions;
    return test::InstallWebApp(profile(), std::move(info));
  }

  FakeWebAppOriginAssociationManager* fake_origin_association_manager() {
    return fake_origin_association_manager_;
  }

  base::SimpleTestClock& clock() { return *clock_.get(); }

  MockWebAppCommandScheduler& mock_scheduler() { return *mock_scheduler_; }

 private:
  raw_ptr<FakeWebAppOriginAssociationManager> fake_origin_association_manager_ =
      nullptr;
  raw_ptr<MockWebAppCommandScheduler> mock_scheduler_ = nullptr;
  std::unique_ptr<base::SimpleTestClock> clock_;
};

TEST_F(UpdateValidatedOriginAssociationsCommandTest, Success) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  // Install will not validate scope extensions.
  fake_origin_association_manager()->set_pass_through(false);
  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(10) + base::Seconds(1));

  base::HistogramTester tester;
  fake_origin_association_manager()->set_pass_through(true);

  MockWebAppRegistrarObserver observer;
  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver> observation(
      &observer);
  observation.Observe(&provider().registrar_unsafe());

  EXPECT_CALL(observer, OnWebAppEffectiveScopeChanged(app_id, _));

  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;

  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future.Get());

  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kSuccess,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(app->validated_scope_extensions().empty());
  EXPECT_EQ(extension, *app->validated_scope_extensions().begin());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, UnvalidatedItemsRemain) {
  GURL start_url("https://example.com/");

  // Fake manager defaults to returning empty associations, which means failure
  // if we have unvalidated extensions.
  fake_origin_association_manager()->set_pass_through(false);

  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});
  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_TRUE(app->validated_scope_extensions().empty());
    EXPECT_TRUE(
        app->origin_association_last_validation_check_time().has_value());
  }

  base::HistogramTester tester;
  clock().Advance(base::Days(10) + base::Seconds(1));
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            future.Get());

  tester.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, ThrottledAfterInstall) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});

  // Fake manager defaults to returning empty associations, which means failure
  // if we have unvalidated extensions.
  fake_origin_association_manager()->set_pass_through(false);

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kThrottled, future.Get());

  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kThrottled,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, ThrottledAfterRevalidate) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  fake_origin_association_manager()->set_pass_through(false);

  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(10) + base::Seconds(1));

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            future.Get());
  tester.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);

  base::HistogramTester tester2;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future2;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future2.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kThrottled, future2.Get());
  tester2.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, ThrottledWhenNoTimeValue) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});

  // Clear the time value.
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetOriginAssociationLastValidationCheckTime(std::nullopt);
  }

  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_FALSE(
        app->origin_association_last_validation_check_time().has_value());
  }

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kThrottled, future.Get());

  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kThrottled,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
  // Backfill must land within the past day: in the past, so the app is not
  // deferred beyond the throttle interval, and no further back than a day, so
  // it is still throttled now. Both bounds matter -- asserting only one lets
  // the sign of the offset flip without failing.
  EXPECT_LE(*app->origin_association_last_validation_check_time(),
            clock().Now());
  EXPECT_GE(*app->origin_association_last_validation_check_time(),
            clock().Now() - base::Days(1));
}

// The randomized backfill exists to spread first fetches over 24 hours.
// Whatever point in that window an app lands on, it must become eligible within
// a day -- otherwise a revoked association stays trusted longer than the
// throttle promises.
TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       NotThrottledWithinADayOfBackfill) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_id)->SetOriginAssociationLastValidationCheckTime(
        std::nullopt);
  }

  // First call only backfills the timestamp.
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> backfill;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, backfill.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kThrottled,
            backfill.Get());

  clock().Advance(base::Days(1) + base::Seconds(1));

  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  EXPECT_NE(UpdateValidatedOriginAssociationsResult::kThrottled, future.Get());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, NotThrottleAfterTenDays) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  fake_origin_association_manager()->set_pass_through(false);
  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(10) + base::Seconds(1));

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            future.Get());
  tester.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);

  fake_origin_association_manager()->set_pass_through(true);
  clock().Advance(base::Days(10) + base::Seconds(1));

  base::HistogramTester tester2;

  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future2;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future2.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future2.Get());

  tester2.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                             UpdateValidatedOriginAssociationsResult::kSuccess,
                             1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, EmptyReturnsThrottled) {
  GURL start_url("https://example.com/");
  // No scope extensions.
  webapps::AppId app_id = InstallApp(start_url, {});
  clock().Advance(base::Days(10) + base::Seconds(1));

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());

  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kThrottled, future.Get());
  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kThrottled,
                            1);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       RevalidatesAlreadyValidatedScopeExtension) {
  fake_origin_association_manager()->set_pass_through(true);

  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(10) + base::Seconds(1));

  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_FALSE(app->validated_scope_extensions().empty());
    EXPECT_FALSE(app->scope_extensions().empty());
  }

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;

  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future.Get());
  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kSuccess,
                            1);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       MissingAppReturnsNotInstalled) {
  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      "non-existent-app-id", future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kWebAppNotInstalled,
            future.Get());
  tester.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kWebAppNotInstalled, 1);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, MigrationSourcesSuccess) {
  GURL start_url("https://example.com/");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Test App";
  webapps::ManifestId manifest_id = info->manifest_id();
  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));

  MigrationSource migration_source(manifest_id, MigrationBehavior::kForce,
                                   GURL("https://example.com/subpath"));

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetUnvalidatedMigrationSources({migration_source});
  }

  base::HistogramTester tester;
  fake_origin_association_manager()->set_pass_through(true);
  clock().Advance(base::Days(10) + base::Seconds(1));

  EXPECT_CALL(mock_scheduler(), ScheduleResolveWebAppPendingMigrationInfo(_, _))
      .Times(1);

  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future.Get());
  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kSuccess,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(app->validated_migration_sources().empty());
  EXPECT_EQ(migration_source, *app->validated_migration_sources().begin());

  // Still success on repeatable validation.
  base::HistogramTester tester2;
  clock().Advance(base::Days(10) + base::Seconds(1));
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future2;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future2.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future2.Get());
  tester2.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                             UpdateValidatedOriginAssociationsResult::kSuccess,
                             1);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       SuggestedForMigrationSuccess) {
  GURL start_url("https://example.com/");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Test App";
  webapps::ManifestId manifest_id = info->manifest_id();
  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));

  MigrationSource migration_source(manifest_id, MigrationBehavior::kForce,
                                   GURL("https://example.com/subpath"));

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetInstallState(
        proto::InstallState::SUGGESTED_FROM_MIGRATION);
    app_to_update->SetUnvalidatedMigrationSources({migration_source});
  }

  base::HistogramTester tester;
  fake_origin_association_manager()->set_pass_through(true);
  clock().Advance(base::Days(10) + base::Seconds(1));

  EXPECT_CALL(mock_scheduler(), ScheduleResolveWebAppPendingMigrationInfo(_, _))
      .Times(1);

  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future.Get());
  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kSuccess,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(app->validated_migration_sources().empty());
  EXPECT_EQ(migration_source, *app->validated_migration_sources().begin());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       RemoveStaleScopeExtension) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension1 = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);
  ScopeExtensionInfo extension2 = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.com/scope"), /*has_origin_wildcard=*/false);

  // Install will not validate scope extensions.
  fake_origin_association_manager()->set_pass_through(false);
  webapps::AppId app_id = InstallApp(start_url, {extension1, extension2});
  clock().Advance(base::Days(10) + base::Seconds(1));

  // First validation: both extensions are valid.
  fake_origin_association_manager()->SetData(
      {{extension1, extension1}, {extension2, extension2}});

  {
    base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
    provider().scheduler().UpdateValidatedOriginAssociations(
        app_id, future.GetCallback());
    ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future.Get());

    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_EQ(app->validated_scope_extensions().size(), 2u);
  }

  // Advance clock to bypass throttling.
  clock().Advance(base::Days(10) + base::Seconds(1));

  // Second validation: extension1 is no longer valid (removed from association
  // file).
  fake_origin_association_manager()->SetData({{extension2, extension2}});

  {
    base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
    provider().scheduler().UpdateValidatedOriginAssociations(
        app_id, future.GetCallback());
    ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
              future.Get());

    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_EQ(app->validated_scope_extensions().size(), 1u);
    EXPECT_EQ(extension2, *app->validated_scope_extensions().begin());
  }
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       RemoveStaleMigrationSource) {
  GURL start_url("https://example.com/");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Test App";
  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));

  MigrationSource migration_source1(
      webapps::ManifestId(GURL("https://example.org/manifest.json")),
      MigrationBehavior::kForce, GURL("https://example.org/subpath"));
  MigrationSource migration_source2(
      webapps::ManifestId(GURL("https://example.com/manifest.json")),
      MigrationBehavior::kForce, GURL("https://example.com/subpath"));

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetUnvalidatedMigrationSources(
        {migration_source1, migration_source2});
  }

  fake_origin_association_manager()->set_pass_through(false);
  clock().Advance(base::Days(10) + base::Seconds(1));

  // First validation: both migration sources are valid.
  fake_origin_association_manager()->SetMigrationSourcesData(
      {migration_source1.manifest_id(), migration_source2.manifest_id()});

  EXPECT_CALL(mock_scheduler(), ScheduleResolveWebAppPendingMigrationInfo(_, _))
      .Times(2);

  {
    base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
    provider().scheduler().UpdateValidatedOriginAssociations(
        app_id, future.GetCallback());
    ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future.Get());

    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_EQ(app->validated_migration_sources().size(), 2u);
  }

  // Advance clock to bypass throttling.
  clock().Advance(base::Days(10) + base::Seconds(1));

  // Second validation: migration_source1 is no longer valid.
  fake_origin_association_manager()->SetMigrationSourcesData(
      {migration_source2.manifest_id()});

  {
    base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
    provider().scheduler().UpdateValidatedOriginAssociations(
        app_id, future.GetCallback());
    ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
              future.Get());

    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_EQ(app->validated_migration_sources().size(), 1u);
    EXPECT_EQ(migration_source2, *app->validated_migration_sources().begin());
  }
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, Offline) {
  auto mock_network_change_notifier =
      std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kOffline, future.Get());

  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kOffline,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, IWARateLimiting) {
  GURL start_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  fake_origin_association_manager()->set_pass_through(false);

  auto app = test::CreateWebApp(start_url);
  app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());
  app->SetScopeExtensions({extension});
  app->SetOriginAssociationLastValidationCheckTime(clock().Now());

  webapps::AppId app_id = app->app_id();
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(app));
  }

  // Throttled if checked within 1 day.
  clock().Advance(base::Hours(12));

  base::HistogramTester tester;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(UpdateValidatedOriginAssociationsResult::kThrottled, future.Get());
  tester.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                            UpdateValidatedOriginAssociationsResult::kThrottled,
                            1);

  // Allowed after 1 day.
  fake_origin_association_manager()->set_pass_through(true);
  clock().Advance(base::Days(1) + base::Seconds(1));

  base::HistogramTester tester2;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> future2;
  provider().scheduler().UpdateValidatedOriginAssociations(
      app_id, future2.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kSuccess, future2.Get());
  tester2.ExpectUniqueSample("WebApp.ValidatedOriginAssociations.Updated",
                             UpdateValidatedOriginAssociationsResult::kSuccess,
                             1);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       SerializesConcurrentRequestsAtCooldownBoundary) {
  const auto app_id =
      InstallApp(GURL("https://example.com/"),
                 {ScopeExtensionInfo::CreateForOrigin(
                     url::Origin::Create(GURL("https://example.org/")))});
  fake_origin_association_manager()->set_pass_through(true);
  const base::Time last_check =
      *provider()
           .registrar_unsafe()
           .GetAppById(app_id)
           ->origin_association_last_validation_check_time();
  base::HistogramTester histograms;

  clock().SetNow(last_check + base::Days(1) - base::Milliseconds(1));
  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);

  clock().SetNow(last_check + base::Days(1));
  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 2);

  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 3);

  clock().Advance(base::Days(1));
  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kSuccess, 2);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 5);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest, MissingAndIneligibleApps) {
  const auto app_id = InstallApp(GURL("https://example.com/"), {});
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_id)->SetOriginAssociationLastValidationCheckTime(
        std::nullopt);
  }
  clock().Advance(base::Days(2));
  base::HistogramTester histograms;
  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().scheduler().UpdateValidatedOriginAssociations("missing",
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kWebAppNotInstalled, 1);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 2);
  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppById(app_id)
                   ->origin_association_last_validation_check_time()
                   .has_value());
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       CanRetryAfterOfflineCompletion) {
  const auto app_id =
      InstallApp(GURL("https://example.com/"),
                 {ScopeExtensionInfo::CreateForOrigin(
                     url::Origin::Create(GURL("https://example.org/")))});
  clock().Advance(base::Days(2));
  base::HistogramTester histograms;
  net::test::ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kOffline, 1);

  notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  fake_origin_association_manager()->set_pass_through(true);
  provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kSuccess, 1);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 2);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       IwaRequiresFeatureAndScopeExtensions) {
  auto app = test::CreateWebApp(
      GURL("isolated-app://"
           "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"));
  app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());
  const base::Time last_check = clock().Now() - base::Days(2);
  app->SetOriginAssociationLastValidationCheckTime(last_check);
  const auto app_id = app->app_id();
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(app));
  }
  fake_origin_association_manager()->set_pass_through(true);
  base::HistogramTester histograms;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        blink::features::kWebAppEnableScopeExtensionsForIsolatedWebApps);
    provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                             base::DoNothing());
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    histograms.ExpectUniqueSample(
        "WebApp.ValidatedOriginAssociations.Updated",
        UpdateValidatedOriginAssociationsResult::kThrottled, 1);
    EXPECT_EQ(last_check,
              provider()
                  .registrar_unsafe()
                  .GetAppById(app_id)
                  ->origin_association_last_validation_check_time());
  }
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_id)->SetScopeExtensions(
        {ScopeExtensionInfo::CreateForOrigin(
            url::Origin::Create(GURL("https://example.org/")))});
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        blink::features::kWebAppEnableScopeExtensionsForIsolatedWebApps);
    provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                             base::DoNothing());
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    histograms.ExpectUniqueSample(
        "WebApp.ValidatedOriginAssociations.Updated",
        UpdateValidatedOriginAssociationsResult::kThrottled, 2);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        blink::features::kWebAppEnableScopeExtensionsForIsolatedWebApps);
    provider().scheduler().UpdateValidatedOriginAssociations(app_id,
                                                             base::DoNothing());
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    histograms.ExpectBucketCount(
        "WebApp.ValidatedOriginAssociations.Updated",
        UpdateValidatedOriginAssociationsResult::kSuccess, 1);
    histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated",
                                3);
  }
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       MigrationDestinationFreshnessIsIndependentOfSource) {
  const auto source_id = InstallApp(GURL("https://source.example/"), {});
  const auto destination_id =
      InstallApp(GURL("https://destination.example/"), {});
  const auto source_manifest =
      provider().registrar_unsafe().GetAppById(source_id)->manifest_id();
  const auto destination_manifest =
      provider().registrar_unsafe().GetAppById(destination_id)->manifest_id();
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    auto* source = update->UpdateApp(source_id);
    source->SetPendingMigrationInfo(PendingMigrationInfo(
        destination_manifest, MigrationBehavior::kSuggest));
    source->SetOriginAssociationLastValidationCheckTime(clock().Now());
    auto* destination = update->UpdateApp(destination_id);
    destination->SetInstallState(proto::InstallState::SUGGESTED_FROM_MIGRATION);
    const std::vector<MigrationSource> sources = {
        MigrationSource(source_manifest, MigrationBehavior::kSuggest)};
    destination->SetUnvalidatedMigrationSources(sources);
    destination->SetValidatedMigrationSources(sources);
    destination->SetOriginAssociationLastValidationCheckTime(clock().Now() -
                                                             base::Days(2));
  }

  fake_origin_association_manager()->set_pass_through(false);
  base::HistogramTester histograms;
  provider().scheduler().UpdateValidatedOriginAssociations(source_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppById(destination_id)
                  ->validated_migration_sources()
                  .empty());
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 2);

  // A recent, eligible source must not suppress the destination either.
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(source_id)->SetScopeExtensions(
        {ScopeExtensionInfo::CreateForOrigin(
            url::Origin::Create(GURL("https://example.org/")))});
    update->UpdateApp(destination_id)
        ->SetOriginAssociationLastValidationCheckTime(clock().Now() -
                                                      base::Days(2));
  }
  provider().scheduler().UpdateValidatedOriginAssociations(source_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 2);
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 2);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 4);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       RecentMigrationDestinationDoesNotThrottleSource) {
  const base::Time now = clock().Now().UTCMidnight();
  clock().SetNow(now);
  const base::Time destination_last_check = now - base::Hours(1);
  const ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForOrigin(
      url::Origin::Create(GURL("https://extended.example/")));
  fake_origin_association_manager()->set_pass_through(true);
  const auto source_id =
      InstallApp(GURL("https://source.example/"), {extension});
  ASSERT_THAT(provider()
                  .registrar_unsafe()
                  .GetAppById(source_id)
                  ->validated_scope_extensions(),
              testing::ElementsAre(extension));
  const auto destination_id =
      InstallApp(GURL("https://destination.example/"), {});
  const auto source_manifest =
      provider().registrar_unsafe().GetAppById(source_id)->manifest_id();
  const auto destination_manifest =
      provider().registrar_unsafe().GetAppById(destination_id)->manifest_id();
  const std::vector<MigrationSource> migration_sources = {
      MigrationSource(source_manifest, MigrationBehavior::kSuggest)};
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    auto* source = update->UpdateApp(source_id);
    source->SetPendingMigrationInfo(PendingMigrationInfo(
        destination_manifest, MigrationBehavior::kSuggest));
    source->SetOriginAssociationLastValidationCheckTime(now - base::Days(2));
    auto* destination = update->UpdateApp(destination_id);
    destination->SetUnvalidatedMigrationSources(migration_sources);
    destination->SetValidatedMigrationSources(migration_sources);
    destination->SetOriginAssociationLastValidationCheckTime(
        destination_last_check);
  }
  fake_origin_association_manager()->set_pass_through(false);

  base::HistogramTester histograms;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> result;
  provider().scheduler().UpdateValidatedOriginAssociations(
      source_id, result.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            result.Get());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  const WebApp* source = provider().registrar_unsafe().GetAppById(source_id);
  EXPECT_TRUE(source->validated_scope_extensions().empty());
  EXPECT_EQ(now, source->origin_association_last_validation_check_time());
  const WebApp* destination =
      provider().registrar_unsafe().GetAppById(destination_id);
  EXPECT_EQ(migration_sources, destination->validated_migration_sources());
  EXPECT_EQ(destination_last_check,
            destination->origin_association_last_validation_check_time());
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 2);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       SourceTimestampBackfillDoesNotSuppressMigrationDestination) {
  const base::Time now = clock().Now().UTCMidnight();
  clock().SetNow(now);
  const ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForOrigin(
      url::Origin::Create(GURL("https://extended.example/")));
  fake_origin_association_manager()->set_pass_through(true);
  const auto source_id =
      InstallApp(GURL("https://source.example/"), {extension});
  const auto destination_id =
      InstallApp(GURL("https://destination.example/"), {});
  const auto source_manifest =
      provider().registrar_unsafe().GetAppById(source_id)->manifest_id();
  const auto destination_manifest =
      provider().registrar_unsafe().GetAppById(destination_id)->manifest_id();
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    auto* source = update->UpdateApp(source_id);
    source->SetPendingMigrationInfo(PendingMigrationInfo(
        destination_manifest, MigrationBehavior::kSuggest));
    source->SetOriginAssociationLastValidationCheckTime(std::nullopt);
    auto* destination = update->UpdateApp(destination_id);
    const std::vector<MigrationSource> migration_sources = {
        MigrationSource(source_manifest, MigrationBehavior::kSuggest)};
    destination->SetUnvalidatedMigrationSources(migration_sources);
    destination->SetValidatedMigrationSources(migration_sources);
    destination->SetOriginAssociationLastValidationCheckTime(now -
                                                             base::Days(2));
  }
  fake_origin_association_manager()->set_pass_through(false);

  base::HistogramTester histograms;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> result;
  provider().scheduler().UpdateValidatedOriginAssociations(
      source_id, result.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kThrottled, result.Get());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  const WebApp* source = provider().registrar_unsafe().GetAppById(source_id);
  EXPECT_TRUE(
      source->origin_association_last_validation_check_time().has_value());
  EXPECT_THAT(source->validated_scope_extensions(),
              testing::ElementsAre(extension));
  const WebApp* destination =
      provider().registrar_unsafe().GetAppById(destination_id);
  EXPECT_TRUE(destination->validated_migration_sources().empty());
  EXPECT_EQ(now, destination->origin_association_last_validation_check_time());
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 2);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       MissingMigrationDestinationDoesNotPreventSourceRevalidation) {
  const base::Time now = clock().Now().UTCMidnight();
  clock().SetNow(now);
  const ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForOrigin(
      url::Origin::Create(GURL("https://extended.example/")));
  fake_origin_association_manager()->set_pass_through(true);
  const auto source_id =
      InstallApp(GURL("https://source.example/"), {extension});
  ASSERT_THAT(provider()
                  .registrar_unsafe()
                  .GetAppById(source_id)
                  ->validated_scope_extensions(),
              testing::ElementsAre(extension));
  const webapps::ManifestId missing_manifest(GURL("https://missing.example/"));
  ASSERT_FALSE(provider().registrar_unsafe().GetAppById(
      GenerateAppIdFromManifestId(missing_manifest)));
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    auto* source = update->UpdateApp(source_id);
    source->SetPendingMigrationInfo(
        PendingMigrationInfo(missing_manifest, MigrationBehavior::kSuggest));
    source->SetOriginAssociationLastValidationCheckTime(now - base::Days(2));
  }
  fake_origin_association_manager()->set_pass_through(false);

  base::HistogramTester histograms;
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> result;
  provider().scheduler().UpdateValidatedOriginAssociations(
      source_id, result.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            result.Get());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  const WebApp* source = provider().registrar_unsafe().GetAppById(source_id);
  EXPECT_TRUE(source->validated_scope_extensions().empty());
  EXPECT_EQ(now, source->origin_association_last_validation_check_time());
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kWebAppNotInstalled, 1);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 2);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       MigrationChainRevalidatesNextDestinationOnLaterRequest) {
  const base::Time first_check = clock().Now().UTCMidnight();
  clock().SetNow(first_check);
  const auto app_a_id = InstallApp(GURL("https://example.com/a"), {});
  const auto app_b_id = InstallApp(GURL("https://example.com/b"), {});
  const auto app_c_id = InstallApp(GURL("https://example.com/c"), {});
  const auto a_manifest =
      provider().registrar_unsafe().GetAppById(app_a_id)->manifest_id();
  const auto b_manifest =
      provider().registrar_unsafe().GetAppById(app_b_id)->manifest_id();
  const auto c_manifest =
      provider().registrar_unsafe().GetAppById(app_c_id)->manifest_id();
  const PendingMigrationInfo b_to_c(c_manifest, MigrationBehavior::kSuggest);
  const base::Time stale_check = first_check - base::Days(2);
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_a_id)->SetPendingMigrationInfo(
        PendingMigrationInfo(b_manifest, MigrationBehavior::kSuggest));
    auto* app_b = update->UpdateApp(app_b_id);
    const std::vector<MigrationSource> b_sources = {
        MigrationSource(a_manifest, MigrationBehavior::kSuggest)};
    app_b->SetUnvalidatedMigrationSources(b_sources);
    app_b->SetValidatedMigrationSources(b_sources);
    app_b->SetPendingMigrationInfo(b_to_c);
    app_b->SetOriginAssociationLastValidationCheckTime(stale_check);
    auto* app_c = update->UpdateApp(app_c_id);
    const std::vector<MigrationSource> c_sources = {
        MigrationSource(b_manifest, MigrationBehavior::kSuggest)};
    app_c->SetUnvalidatedMigrationSources(c_sources);
    app_c->SetValidatedMigrationSources(c_sources);
    app_c->SetOriginAssociationLastValidationCheckTime(stale_check);
  }
  fake_origin_association_manager()->set_pass_through(true);

  // A's request checks B, but must not continue from B to C.
  base::HistogramTester from_a_histograms;
  provider().scheduler().UpdateValidatedOriginAssociations(app_a_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(first_check, provider()
                             .registrar_unsafe()
                             .GetAppById(app_b_id)
                             ->origin_association_last_validation_check_time());
  EXPECT_EQ(stale_check, provider()
                             .registrar_unsafe()
                             .GetAppById(app_c_id)
                             ->origin_association_last_validation_check_time());
  EXPECT_THAT(provider()
                  .registrar_unsafe()
                  .GetAppById(app_b_id)
                  ->pending_migration_info(),
              testing::Optional(b_to_c));
  from_a_histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kSuccess, 1);
  from_a_histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);
  from_a_histograms.ExpectTotalCount(
      "WebApp.ValidatedOriginAssociations.Updated", 2);

  // A later request for B checks C even while B is still within its cooldown.
  clock().Advance(base::Hours(1));
  base::HistogramTester from_b_histograms;
  provider().scheduler().UpdateValidatedOriginAssociations(app_b_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(first_check, provider()
                             .registrar_unsafe()
                             .GetAppById(app_b_id)
                             ->origin_association_last_validation_check_time());
  EXPECT_EQ(clock().Now(),
            provider()
                .registrar_unsafe()
                .GetAppById(app_c_id)
                ->origin_association_last_validation_check_time());
  EXPECT_THAT(provider()
                  .registrar_unsafe()
                  .GetAppById(app_b_id)
                  ->pending_migration_info(),
              testing::Optional(b_to_c));
  from_b_histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kSuccess, 1);
  from_b_histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 1);
  from_b_histograms.ExpectTotalCount(
      "WebApp.ValidatedOriginAssociations.Updated", 2);
}

TEST_F(UpdateValidatedOriginAssociationsCommandTest,
       MigrationCycleDoesNotScheduleRecursively) {
  const auto source_id = InstallApp(GURL("https://source.example/"), {});
  const auto destination_id =
      InstallApp(GURL("https://destination.example/"), {});
  const auto source_manifest =
      provider().registrar_unsafe().GetAppById(source_id)->manifest_id();
  const auto destination_manifest =
      provider().registrar_unsafe().GetAppById(destination_id)->manifest_id();
  {
    auto update = provider().sync_bridge_unsafe().BeginUpdate();
    auto* source = update->UpdateApp(source_id);
    source->SetPendingMigrationInfo(PendingMigrationInfo(
        destination_manifest, MigrationBehavior::kSuggest));
    source->SetUnvalidatedMigrationSources(
        {MigrationSource(destination_manifest, MigrationBehavior::kSuggest)});
    auto* destination = update->UpdateApp(destination_id);
    destination->SetPendingMigrationInfo(
        PendingMigrationInfo(source_manifest, MigrationBehavior::kSuggest));
    destination->SetUnvalidatedMigrationSources(
        {MigrationSource(source_manifest, MigrationBehavior::kSuggest)});
  }
  clock().Advance(base::Days(2));
  fake_origin_association_manager()->set_pass_through(true);
  base::HistogramTester histograms;
  provider().scheduler().UpdateValidatedOriginAssociations(source_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectUniqueSample(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kSuccess, 2);

  provider().scheduler().UpdateValidatedOriginAssociations(source_id,
                                                           base::DoNothing());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectBucketCount(
      "WebApp.ValidatedOriginAssociations.Updated",
      UpdateValidatedOriginAssociationsResult::kThrottled, 2);
  histograms.ExpectTotalCount("WebApp.ValidatedOriginAssociations.Updated", 4);
}

}  // namespace web_app
