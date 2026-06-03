// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/update_validated_origin_associations_command.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/scheduler/update_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
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
#include "third_party/blink/public/common/features.h"

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
        std::make_unique<FakeWebAppOriginAssociationManager>();
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
  // The time should be within the last 10 days.
  EXPECT_GE(*app->origin_association_last_validation_check_time(),
            clock().Now());
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

TEST_F(UpdateValidatedOriginAssociationsCommandTest, EmptyReturnsSuccess) {
  GURL start_url("https://example.com/");
  // No scope extensions.
  webapps::AppId app_id = InstallApp(start_url, {});
  clock().Advance(base::Days(10) + base::Seconds(1));

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
       ValidateTwoTimesStillSuccess) {
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

TEST_F(UpdateValidatedOriginAssociationsCommandTest, AppDisabled) {
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kWebAppMigrationApi);

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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kWebAppMigrationApi);

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

}  // namespace web_app
